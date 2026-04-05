#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "constants.h"
#include "stream.h"

// ---------------------------------------------------------------------------
// World file layout (column-major):
//   byte offset = col * WORLD_H + row
// RING_W and RING_H are powers of 2 so all modulos use bitwise AND.
// ---------------------------------------------------------------------------

#define RING_W_MASK  (RING_W - 1)   // 63
#define RING_H_MASK  (RING_H - 1)   // 31

static int s_fg_fd = -1;    // column-major: offset = col * WORLD_H + row
static int s_bg_fd = -1;
static int s_fg_row_fd = -1; // row-major:    offset = row * WORLD_W + col
static int s_bg_row_fd = -1;

// Top-left world-tile coordinates of the currently loaded ring-buffer window.
static uint16_t s_loaded_left = 0;
static uint16_t s_loaded_top  = 0;

// ---------------------------------------------------------------------------
// Staging buffers — USB reads happen during vsync spin-wait (stream_prefetch);
// XRAM writes happen at vsync in stream_commit.  Each holds at most one
// pending column or row per vsync period.
// ---------------------------------------------------------------------------
static uint8_t  s_stage_fg_col[RING_H];
static uint8_t  s_stage_bg_col[RING_H];
static uint16_t s_stage_col_idx;       // world col being staged
static uint16_t s_stage_col_row0;      // s_loaded_top at time of prefetch
static int8_t   s_stage_col_delta;     // +1 right, -1 left
static bool     s_stage_col_pending = false;

static uint8_t  s_stage_fg_row[RING_W];
static uint8_t  s_stage_bg_row[RING_W];
static uint16_t s_stage_row_idx;       // world row being staged
static uint16_t s_stage_row_col0;      // s_loaded_left at time of prefetch
static int8_t   s_stage_row_delta;     // +1 down, -1 up
static bool     s_stage_row_pending = false;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Write a column of tiles into one ring-buffer layer.
// world_row_start is the world row of tiles[0]; each tile goes to the correct
// ring row (world_row % RING_H) so the VGA wrap arithmetic works correctly.
static void write_ring_col(unsigned tilemap_base, uint16_t world_col,
                            uint16_t world_row_start, const uint8_t *tiles)
{
    uint8_t ring_col  = (uint8_t)(world_col & RING_W_MASK);
    uint8_t ring_row0 = (uint8_t)(world_row_start & RING_H_MASK);
    uint8_t part_a    = RING_H - ring_row0;   // rows until ring wraps

    // Part A: ring rows [ring_row0, RING_H)
    RIA.addr0 = (uint16_t)(tilemap_base + (unsigned)ring_row0 * RING_W + ring_col);
    RIA.step0 = (int8_t)RING_W;
    for (uint8_t i = 0; i < part_a; i++)
        RIA.rw0 = tiles[i];

    // Part B: ring rows [0, ring_row0)  — only needed if wrap occurs
    if (ring_row0 > 0) {
        RIA.addr0 = (uint16_t)(tilemap_base + ring_col);
        RIA.step0 = (int8_t)RING_W;
        for (uint8_t i = part_a; i < RING_H; i++)
            RIA.rw0 = tiles[i];
    }
}

// Write a row of tiles into one ring-buffer layer.
// tiles[i] is the tile for world col (col_start + i).
// Accounts for ring-column wrap so tiles land at the correct ring slot
// regardless of how far the camera has scrolled horizontally.
static void write_ring_row(unsigned tilemap_base, uint16_t world_row,
                            uint16_t col_start, const uint8_t *tiles)
{
    uint8_t ring_row  = (uint8_t)(world_row & RING_H_MASK);
    uint8_t ring_col0 = (uint8_t)(col_start & RING_W_MASK);
    uint8_t part_a    = RING_W - ring_col0;   // cols before ring wraps

    // Part A: ring cols [ring_col0, RING_W)
    RIA.addr0 = (uint16_t)(tilemap_base + (unsigned)ring_row * RING_W + ring_col0);
    RIA.step0 = 1;
    for (uint8_t i = 0; i < part_a; i++)
        RIA.rw0 = tiles[i];

    // Part B: ring cols [0, ring_col0) — only needed if wrap occurs
    if (ring_col0 > 0) {
        RIA.addr0 = (uint16_t)(tilemap_base + (unsigned)ring_row * RING_W);
        RIA.step0 = 1;
        for (uint8_t i = part_a; i < RING_W; i++)
            RIA.rw0 = tiles[i];
    }
}

// Load column world_col, rows [row_start, row_start + RING_H) from both maze
// files and scatter them into the ring buffers at the correct ring rows.
static void load_column(uint16_t world_col, uint16_t row_start)
{
    uint8_t buf[RING_H];
    long offset = (long)world_col * WORLD_H + row_start;

    lseek(s_fg_fd, offset, SEEK_SET);
    read(s_fg_fd, buf, RING_H);
    write_ring_col(FG_TILEMAP_BASE, world_col, row_start, buf);

    lseek(s_bg_fd, offset, SEEK_SET);
    read(s_bg_fd, buf, RING_H);
    write_ring_col(BG_TILEMAP_BASE, world_col, row_start, buf);
}

// Read row world_row into the staging buffer (USB reads only, no XRAM writes).
// Column-major layout: each tile is 1 byte at offset (col * WORLD_H + world_row),
// so loading a full row requires RING_W separate seeks — done during prefetch.
// Read row world_row into the staging buffer using the row-major files
// (1 seek + RING_W contiguous bytes each, same cost as a column read).
static void stage_row_from_usb(uint16_t world_row, uint16_t col_start, int8_t delta)
{
    long offset = (long)world_row * WORLD_W + col_start;
    lseek(s_fg_row_fd, offset, SEEK_SET);
    read(s_fg_row_fd, s_stage_fg_row, RING_W);
    lseek(s_bg_row_fd, offset, SEEK_SET);
    read(s_bg_row_fd, s_stage_bg_row, RING_W);
    s_stage_row_idx     = world_row;
    s_stage_row_col0    = col_start;
    s_stage_row_delta   = delta;
    s_stage_row_pending = true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int stream_open_files(void)
{
    s_fg_fd = open("MAZE_FG.BIN", O_RDONLY);
    if (s_fg_fd < 0) { puts("stream: MAZE_FG.BIN open FAILED"); return -1; }
    puts("stream: MAZE_FG.BIN opened ok");

    s_bg_fd = open("MAZE_BG.BIN", O_RDONLY);
    if (s_bg_fd < 0) {
        puts("stream: MAZE_BG.BIN open FAILED");
        close(s_fg_fd); s_fg_fd = -1; return -1;
    }
    puts("stream: MAZE_BG.BIN opened ok");

    s_fg_row_fd = open("MAZE_FG_ROWS.BIN", O_RDONLY);
    if (s_fg_row_fd < 0) {
        puts("stream: MAZE_FG_ROWS.BIN open FAILED");
        close(s_fg_fd); s_fg_fd = -1;
        close(s_bg_fd); s_bg_fd = -1; return -1;
    }
    puts("stream: MAZE_FG_ROWS.BIN opened ok");

    s_bg_row_fd = open("MAZE_BG_ROWS.BIN", O_RDONLY);
    if (s_bg_row_fd < 0) {
        puts("stream: MAZE_BG_ROWS.BIN open FAILED");
        close(s_fg_fd); s_fg_fd = -1;
        close(s_bg_fd); s_bg_fd = -1;
        close(s_fg_row_fd); s_fg_row_fd = -1; return -1;
    }
    puts("stream: MAZE_BG_ROWS.BIN opened ok");
    return 0;
}

void stream_init(int16_t cam_x_px, int16_t cam_y_px)
{
    // Clamp camera to valid range
    if (cam_x_px < 0) cam_x_px = 0;
    if (cam_y_px < 0) cam_y_px = 0;

    s_loaded_left = (uint16_t)(cam_x_px / TILE_W);
    s_loaded_top  = (uint16_t)(cam_y_px / TILE_H);

    printf("stream_init: cam_px=(%d,%d) tile=(%u,%u) ring_row0=%u\n",
           cam_x_px, cam_y_px, s_loaded_left, s_loaded_top,
           (unsigned)(s_loaded_top & RING_H_MASK));

    // Fill the entire ring buffer for this window
    for (uint8_t i = 0; i < RING_W; i++) {
        uint16_t col = s_loaded_left + i;
        if (col < WORLD_W)
            load_column(col, s_loaded_top);
    }

    // Sanity-check: print a few tile values from the loaded buffer
    printf("stream_init done. ring tiles at row0: col0=%u col2=%u col10=%u\n",
           (unsigned)stream_read_fg_tile(s_loaded_left,   s_loaded_top),
           (unsigned)stream_read_fg_tile(s_loaded_left+2, s_loaded_top),
           (unsigned)stream_read_fg_tile(s_loaded_left+10, s_loaded_top));
    printf("  ground row tiles: col0=%u col2=%u col10=%u\n",
           (unsigned)stream_read_fg_tile(s_loaded_left,   s_loaded_top+16),
           (unsigned)stream_read_fg_tile(s_loaded_left+2, s_loaded_top+16),
           (unsigned)stream_read_fg_tile(s_loaded_left+10, s_loaded_top+16));
}

// ---------------------------------------------------------------------------
// Two-phase streaming API
// ---------------------------------------------------------------------------

// Phase 1 — call during vsync spin-wait.  Reads USB into staging buffers;
// idempotent once data is staged (fast return).  No XRAM writes.
void stream_prefetch(int16_t cam_x_px, int16_t cam_y_px)
{
    if (s_fg_fd < 0) return;
    if (cam_x_px < 0) cam_x_px = 0;
    if (cam_y_px < 0) cam_y_px = 0;

    uint16_t cam_tile_x = (uint16_t)(cam_x_px / TILE_W);
    uint16_t cam_tile_y = (uint16_t)(cam_y_px / TILE_H);

    // Horizontal: stage one column whenever the camera crosses a tile boundary.
    if (!s_stage_col_pending) {
        if (cam_tile_x > s_loaded_left) {
            // Scrolled right: load the incoming right-edge column.
            uint16_t new_col = s_loaded_left + RING_W;
            if (new_col < WORLD_W) {
                long offset = (long)new_col * WORLD_H + s_loaded_top;
                lseek(s_fg_fd, offset, SEEK_SET);
                read(s_fg_fd, s_stage_fg_col, RING_H);
                lseek(s_bg_fd, offset, SEEK_SET);
                read(s_bg_fd, s_stage_bg_col, RING_H);
                s_stage_col_idx     = new_col;
                s_stage_col_row0    = s_loaded_top;
                s_stage_col_delta   = 1;
                s_stage_col_pending = true;
            } else {
                s_loaded_left++;  // past world right edge
            }
        } else if (s_loaded_left > 0 && cam_tile_x < s_loaded_left) {
            // Scrolled left: load the incoming left-edge column.
            uint16_t new_col = s_loaded_left - 1;
            long offset = (long)new_col * WORLD_H + s_loaded_top;
            lseek(s_fg_fd, offset, SEEK_SET);
            read(s_fg_fd, s_stage_fg_col, RING_H);
            lseek(s_bg_fd, offset, SEEK_SET);
            read(s_bg_fd, s_stage_bg_col, RING_H);
            s_stage_col_idx     = new_col;
            s_stage_col_row0    = s_loaded_top;
            s_stage_col_delta   = -1;
            s_stage_col_pending = true;
        }
    }

    // Vertical: stage one row whenever the camera crosses a tile boundary.
    if (!s_stage_row_pending) {
        if (cam_tile_y > s_loaded_top) {
            // Scrolled down: load the incoming bottom-edge row.
            uint16_t new_row = s_loaded_top + RING_H;
            if (new_row < WORLD_H) {
                stage_row_from_usb(new_row, s_loaded_left, 1);
            } else {
                s_loaded_top++;   // past world bottom edge
            }
        } else if (s_loaded_top > 0 && cam_tile_y < s_loaded_top) {
            // Scrolled up: load the incoming top-edge row.
            stage_row_from_usb(s_loaded_top - 1, s_loaded_left, -1);
        }
    }
}

// Phase 2 — call once, immediately after vsync fires (vblank window).
// Flushes staged buffers into the XRAM ring buffers.
void stream_commit(void)
{
    if (s_stage_col_pending) {
        write_ring_col(FG_TILEMAP_BASE, s_stage_col_idx, s_stage_col_row0, s_stage_fg_col);
        write_ring_col(BG_TILEMAP_BASE, s_stage_col_idx, s_stage_col_row0, s_stage_bg_col);
        if (s_stage_col_delta > 0) s_loaded_left++; else s_loaded_left--;
        s_stage_col_pending = false;
    }

    if (s_stage_row_pending) {
        write_ring_row(FG_TILEMAP_BASE, s_stage_row_idx, s_stage_row_col0, s_stage_fg_row);
        write_ring_row(BG_TILEMAP_BASE, s_stage_row_idx, s_stage_row_col0, s_stage_bg_row);
        if (s_stage_row_delta > 0) s_loaded_top++; else s_loaded_top--;
        s_stage_row_pending = false;
    }
}

void stream_close_files(void)
{
    if (s_fg_fd >= 0)     { close(s_fg_fd);     s_fg_fd     = -1; }
    if (s_bg_fd >= 0)     { close(s_bg_fd);     s_bg_fd     = -1; }
    if (s_fg_row_fd >= 0) { close(s_fg_row_fd); s_fg_row_fd = -1; }
    if (s_bg_row_fd >= 0) { close(s_bg_row_fd); s_bg_row_fd = -1; }
}

uint16_t stream_get_loaded_left(void) { return s_loaded_left; }
uint16_t stream_get_loaded_top(void)  { return s_loaded_top; }

uint8_t stream_read_fg_tile(uint16_t wx, uint16_t wy)
{
    RIA.addr0 = (uint16_t)(FG_TILEMAP_BASE +
                            (uint16_t)((wy & RING_H_MASK) * RING_W) +
                            (uint16_t)(wx & RING_W_MASK));
    RIA.step0 = 0;
    return RIA.rw0;
}
