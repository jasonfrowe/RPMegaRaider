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

// ---------------------------------------------------------------------------
// Cleared-pickup persistence
// When a pickup tile is collected, we record its world coords.
// Before any column or row is written to XRAM from disk data, we patch
// cleared tiles in that slice so they stay empty even after the camera
// scrolls away and back.
// ---------------------------------------------------------------------------
#define MAX_CLEARED_FG 240 // max number of cleared pickups we can track at once (arbitrary limit)
typedef struct { uint16_t wx; uint16_t wy; } cleared_tile_t;
static cleared_tile_t s_cleared_fg[MAX_CLEARED_FG];
static uint8_t        s_cleared_fg_count = 0;

// Patch any cleared-pickup tiles in a column staging buffer.
// col_world: the world column we just loaded into buf[0..RING_H-1].
// row_start: world row index of buf[0].
static void patch_cleared_col(uint8_t *buf, uint16_t col_world, uint16_t row_start)
{
    uint8_t i;
    for (i = 0; i < s_cleared_fg_count; i++) {
        if (s_cleared_fg[i].wx != col_world) continue;
        uint16_t wy = s_cleared_fg[i].wy;
        if (wy < row_start || wy >= row_start + RING_H) continue;
        buf[wy - row_start] = 0;
    }
}

// Patch any cleared-pickup tiles in a row staging buffer.
// row_world: the world row we just loaded into buf[0..RING_W-1].
// col_start: world col index of buf[0].
static void patch_cleared_row(uint8_t *buf, uint16_t row_world, uint16_t col_start)
{
    uint8_t i;
    for (i = 0; i < s_cleared_fg_count; i++) {
        if (s_cleared_fg[i].wy != row_world) continue;
        uint16_t wx = s_cleared_fg[i].wx;
        if (wx < col_start || wx >= col_start + RING_W) continue;
        buf[wx - col_start] = 0;
    }
}

static int s_fg_fd = -1;    // column-major: offset = col * WORLD_H + row
static int s_bg_fd = -1;
static int s_fg_row_fd = -1; // row-major:    offset = row * WORLD_W + col
static int s_bg_row_fd = -1;

// FG and BG are tracked independently.
// BG camera runs at half the FG camera speed (parallax), so it needs
// different world columns/rows loaded into BG_TILEMAP_BASE.
static uint16_t s_fg_loaded_left = 0;
static uint16_t s_fg_loaded_top  = 0;
static uint16_t s_bg_loaded_left = 0;
static uint16_t s_bg_loaded_top  = 0;

// ---------------------------------------------------------------------------
// Staging buffers — USB reads during vsync spin-wait; XRAM writes at vblank.
// FG and BG each have an independent column slot and row slot.
// ---------------------------------------------------------------------------
static uint8_t  s_fg_stage_col[RING_H];
static uint16_t s_fg_stage_col_idx;
static uint16_t s_fg_stage_col_row0;
static int8_t   s_fg_stage_col_delta;
static bool     s_fg_stage_col_pending = false;

static uint8_t  s_bg_stage_col[RING_H];
static uint16_t s_bg_stage_col_idx;
static uint16_t s_bg_stage_col_row0;
static int8_t   s_bg_stage_col_delta;
static bool     s_bg_stage_col_pending = false;

static uint8_t  s_fg_stage_row[RING_W];
static uint16_t s_fg_stage_row_idx;
static uint16_t s_fg_stage_row_col0;
static int8_t   s_fg_stage_row_delta;
static bool     s_fg_stage_row_pending = false;

static uint8_t  s_bg_stage_row[RING_W];
static uint16_t s_bg_stage_row_idx;
static uint16_t s_bg_stage_row_col0;
static int8_t   s_bg_stage_row_delta;
static bool     s_bg_stage_row_pending = false;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Seek and read an entire slice. Returns false if seek/read was short or failed.
static bool read_slice(int fd, long offset, uint8_t *dst, uint16_t len)
{
    if (lseek(fd, offset, SEEK_SET) < 0) return false;
    uint16_t total = 0;
    while (total < len) {
        int got = read(fd, dst + total, (uint16_t)(len - total));
        if (got <= 0) return false;
        total = (uint16_t)(total + (uint16_t)got);
    }
    return true;
}

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

// Direct-load helpers used only during stream_init (before display is active).
static void load_fg_column(uint16_t world_col, uint16_t row_start)
{
    uint8_t buf[RING_H];
    if (read_slice(s_fg_fd, (long)world_col * WORLD_H + row_start, buf, RING_H))
        write_ring_col(FG_TILEMAP_BASE, world_col, row_start, buf);
}

static void load_bg_column(uint16_t world_col, uint16_t row_start)
{
    uint8_t buf[RING_H];
    if (read_slice(s_bg_fd, (long)world_col * WORLD_H + row_start, buf, RING_H))
        write_ring_col(BG_TILEMAP_BASE, world_col, row_start, buf);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int stream_open_files(void)
{
    s_fg_fd = open("ROM:MAZE_FG.BIN", O_RDONLY);
    if (s_fg_fd < 0) { puts("stream: MAZE_FG.BIN open FAILED"); return -1; }
    puts("stream: MAZE_FG.BIN opened ok");

    s_bg_fd = open("ROM:MAZE_BG.BIN", O_RDONLY);
    if (s_bg_fd < 0) {
        puts("stream: MAZE_BG.BIN open FAILED");
        close(s_fg_fd); s_fg_fd = -1; return -1;
    }
    puts("stream: MAZE_BG.BIN opened ok");

    s_fg_row_fd = open("ROM:MAZE_FG_ROWS.BIN", O_RDONLY);
    if (s_fg_row_fd < 0) {
        puts("stream: MAZE_FG_ROWS.BIN open FAILED");
        close(s_fg_fd); s_fg_fd = -1;
        close(s_bg_fd); s_bg_fd = -1; return -1;
    }
    puts("stream: MAZE_FG_ROWS.BIN opened ok");

    s_bg_row_fd = open("ROM:MAZE_BG_ROWS.BIN", O_RDONLY);
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
    if (cam_x_px < 0) cam_x_px = 0;
    if (cam_y_px < 0) cam_y_px = 0;

    // New run/reset: clear all persisted pickup clears and pending staged IO.
    // Otherwise collected bonuses from a previous run remain missing.
    s_cleared_fg_count = 0;
    s_fg_stage_col_pending = false;
    s_bg_stage_col_pending = false;
    s_fg_stage_row_pending = false;
    s_bg_stage_row_pending = false;

    // FG ring: loaded at 1:1 camera position.
    s_fg_loaded_left = (uint16_t)(cam_x_px / TILE_W);
    s_fg_loaded_top  = (uint16_t)(cam_y_px / TILE_H);
    for (uint8_t i = 0; i < RING_W; i++) {
        uint16_t col = s_fg_loaded_left + i;
        if (col < WORLD_W)
            load_fg_column(col, s_fg_loaded_top);
    }

    // BG ring: loaded at half camera position (parallax).
    int16_t bg_cam_x = cam_x_px / 2;
    int16_t bg_cam_y = cam_y_px / 2;
    s_bg_loaded_left = (uint16_t)(bg_cam_x / TILE_W);
    s_bg_loaded_top  = (uint16_t)(bg_cam_y / TILE_H);
    for (uint8_t i = 0; i < RING_W; i++) {
        uint16_t col = s_bg_loaded_left + i;
        if (col < WORLD_W)
            load_bg_column(col, s_bg_loaded_top);
    }

    printf("stream_init: fg_tile=(%u,%u) bg_tile=(%u,%u)\n",
           s_fg_loaded_left, s_fg_loaded_top, s_bg_loaded_left, s_bg_loaded_top);
}

// ---------------------------------------------------------------------------
// Two-phase streaming API
// ---------------------------------------------------------------------------

// Phase 1 — call during vsync spin-wait.  Reads USB into staging buffers;
// FG and BG are staged independently since they move at different rates.
void stream_prefetch(int16_t cam_x_px, int16_t cam_y_px)
{
    if (s_fg_fd < 0) return;
    if (cam_x_px < 0) cam_x_px = 0;
    if (cam_y_px < 0) cam_y_px = 0;

    uint16_t fg_tx = (uint16_t)(cam_x_px / TILE_W);
    uint16_t fg_ty = (uint16_t)(cam_y_px / TILE_H);
    uint16_t bg_tx = (uint16_t)((cam_x_px / 2) / TILE_W);
    uint16_t bg_ty = (uint16_t)((cam_y_px / 2) / TILE_H);

    // Predict the loaded window after any pending/new slice commits so row and
    // column reads line up during diagonal scroll. Without this, a row staged
    // in the same frame as a column can be read from the old left/top origin.
    int8_t fg_col_delta = 0;
    int8_t fg_row_delta = 0;
    int8_t bg_col_delta = 0;
    int8_t bg_row_delta = 0;

    if (s_fg_stage_col_pending) {
        fg_col_delta = s_fg_stage_col_delta;
    } else if (fg_tx > s_fg_loaded_left) {
        uint16_t c = s_fg_loaded_left + RING_W;
        if (c < WORLD_W) fg_col_delta = 1;
    } else if (s_fg_loaded_left > 0 && fg_tx < s_fg_loaded_left) {
        fg_col_delta = -1;
    }

    if (s_fg_stage_row_pending) {
        fg_row_delta = s_fg_stage_row_delta;
    } else if (fg_ty > s_fg_loaded_top) {
        uint16_t r = s_fg_loaded_top + RING_H;
        if (r < WORLD_H) fg_row_delta = 1;
    } else if (s_fg_loaded_top > 0 && fg_ty < s_fg_loaded_top) {
        fg_row_delta = -1;
    }

    if (s_bg_stage_col_pending) {
        bg_col_delta = s_bg_stage_col_delta;
    } else if (bg_tx > s_bg_loaded_left) {
        uint16_t c = s_bg_loaded_left + RING_W;
        if (c < WORLD_W) bg_col_delta = 1;
    } else if (s_bg_loaded_left > 0 && bg_tx < s_bg_loaded_left) {
        bg_col_delta = -1;
    }

    if (s_bg_stage_row_pending) {
        bg_row_delta = s_bg_stage_row_delta;
    } else if (bg_ty > s_bg_loaded_top) {
        uint16_t r = s_bg_loaded_top + RING_H;
        if (r < WORLD_H) bg_row_delta = 1;
    } else if (s_bg_loaded_top > 0 && bg_ty < s_bg_loaded_top) {
        bg_row_delta = -1;
    }

    uint16_t fg_target_left = (fg_col_delta < 0)
        ? (uint16_t)(s_fg_loaded_left - 1u)
        : (uint16_t)(s_fg_loaded_left + (uint16_t)fg_col_delta);
    uint16_t fg_target_top = (fg_row_delta < 0)
        ? (uint16_t)(s_fg_loaded_top - 1u)
        : (uint16_t)(s_fg_loaded_top + (uint16_t)fg_row_delta);
    uint16_t bg_target_left = (bg_col_delta < 0)
        ? (uint16_t)(s_bg_loaded_left - 1u)
        : (uint16_t)(s_bg_loaded_left + (uint16_t)bg_col_delta);
    uint16_t bg_target_top = (bg_row_delta < 0)
        ? (uint16_t)(s_bg_loaded_top - 1u)
        : (uint16_t)(s_bg_loaded_top + (uint16_t)bg_row_delta);

    // --- FG horizontal ---
    if (!s_fg_stage_col_pending) {
        if (fg_tx > s_fg_loaded_left) {
            uint16_t c = s_fg_loaded_left + RING_W;
            if (c < WORLD_W) {
                if (read_slice(s_fg_fd, (long)c * WORLD_H + fg_target_top, s_fg_stage_col, RING_H)) {
                    patch_cleared_col(s_fg_stage_col, c, fg_target_top);
                    s_fg_stage_col_idx = c; s_fg_stage_col_row0 = fg_target_top;
                    s_fg_stage_col_delta = 1; s_fg_stage_col_pending = true;
                }
            }
            // At boundary: don't advance tracking without data
        } else if (s_fg_loaded_left > 0 && fg_tx < s_fg_loaded_left) {
            uint16_t c = s_fg_loaded_left - 1;
            if (read_slice(s_fg_fd, (long)c * WORLD_H + fg_target_top, s_fg_stage_col, RING_H)) {
                patch_cleared_col(s_fg_stage_col, c, fg_target_top);
                s_fg_stage_col_idx = c; s_fg_stage_col_row0 = fg_target_top;
                s_fg_stage_col_delta = -1; s_fg_stage_col_pending = true;
            }
        }
    }

    // --- BG horizontal (half camera speed) ---
    if (!s_bg_stage_col_pending) {
        if (bg_tx > s_bg_loaded_left) {
            uint16_t c = s_bg_loaded_left + RING_W;
            if (c < WORLD_W) {
                if (read_slice(s_bg_fd, (long)c * WORLD_H + bg_target_top, s_bg_stage_col, RING_H)) {
                    s_bg_stage_col_idx = c; s_bg_stage_col_row0 = bg_target_top;
                    s_bg_stage_col_delta = 1; s_bg_stage_col_pending = true;
                }
            }
            // At boundary: don't advance tracking without data
        } else if (s_bg_loaded_left > 0 && bg_tx < s_bg_loaded_left) {
            uint16_t c = s_bg_loaded_left - 1;
            if (read_slice(s_bg_fd, (long)c * WORLD_H + bg_target_top, s_bg_stage_col, RING_H)) {
                s_bg_stage_col_idx = c; s_bg_stage_col_row0 = bg_target_top;
                s_bg_stage_col_delta = -1; s_bg_stage_col_pending = true;
            }
        }
    }

    // --- FG vertical ---
    if (!s_fg_stage_row_pending) {
        if (fg_ty > s_fg_loaded_top) {
            uint16_t r = s_fg_loaded_top + RING_H;
            if (r < WORLD_H) {
                if (read_slice(s_fg_row_fd, (long)r * WORLD_W + fg_target_left, s_fg_stage_row, RING_W)) {
                    patch_cleared_row(s_fg_stage_row, r, fg_target_left);
                    s_fg_stage_row_idx = r; s_fg_stage_row_col0 = fg_target_left;
                    s_fg_stage_row_delta = 1; s_fg_stage_row_pending = true;
                }
            }
            // At boundary: don't advance tracking without data
        } else if (s_fg_loaded_top > 0 && fg_ty < s_fg_loaded_top) {
            uint16_t r = s_fg_loaded_top - 1;
            if (read_slice(s_fg_row_fd, (long)r * WORLD_W + fg_target_left, s_fg_stage_row, RING_W)) {
                patch_cleared_row(s_fg_stage_row, r, fg_target_left);
                s_fg_stage_row_idx = r; s_fg_stage_row_col0 = fg_target_left;
                s_fg_stage_row_delta = -1; s_fg_stage_row_pending = true;
            }
        }
    }

    // --- BG vertical (half camera speed) ---
    if (!s_bg_stage_row_pending) {
        if (bg_ty > s_bg_loaded_top) {
            uint16_t r = s_bg_loaded_top + RING_H;
            if (r < WORLD_H) {
                if (read_slice(s_bg_row_fd, (long)r * WORLD_W + bg_target_left, s_bg_stage_row, RING_W)) {
                    s_bg_stage_row_idx = r; s_bg_stage_row_col0 = bg_target_left;
                    s_bg_stage_row_delta = 1; s_bg_stage_row_pending = true;
                }
            }
            // At boundary: don't advance tracking without data
        } else if (s_bg_loaded_top > 0 && bg_ty < s_bg_loaded_top) {
            uint16_t r = s_bg_loaded_top - 1;
            if (read_slice(s_bg_row_fd, (long)r * WORLD_W + bg_target_left, s_bg_stage_row, RING_W)) {
                s_bg_stage_row_idx = r; s_bg_stage_row_col0 = bg_target_left;
                s_bg_stage_row_delta = -1; s_bg_stage_row_pending = true;
            }
        }
    }
}

// Phase 2 — call once at vblank.  Flushes all staged buffers into XRAM.
void stream_commit(void)
{
    if (s_fg_stage_col_pending) {
        write_ring_col(FG_TILEMAP_BASE, s_fg_stage_col_idx, s_fg_stage_col_row0, s_fg_stage_col);
        if (s_fg_stage_col_delta > 0) s_fg_loaded_left++; else s_fg_loaded_left--;
        s_fg_stage_col_pending = false;
    }
    if (s_bg_stage_col_pending) {
        write_ring_col(BG_TILEMAP_BASE, s_bg_stage_col_idx, s_bg_stage_col_row0, s_bg_stage_col);
        if (s_bg_stage_col_delta > 0) s_bg_loaded_left++; else s_bg_loaded_left--;
        s_bg_stage_col_pending = false;
    }
    if (s_fg_stage_row_pending) {
        write_ring_row(FG_TILEMAP_BASE, s_fg_stage_row_idx, s_fg_stage_row_col0, s_fg_stage_row);
        if (s_fg_stage_row_delta > 0) s_fg_loaded_top++; else s_fg_loaded_top--;
        s_fg_stage_row_pending = false;
    }
    if (s_bg_stage_row_pending) {
        write_ring_row(BG_TILEMAP_BASE, s_bg_stage_row_idx, s_bg_stage_row_col0, s_bg_stage_row);
        if (s_bg_stage_row_delta > 0) s_bg_loaded_top++; else s_bg_loaded_top--;
        s_bg_stage_row_pending = false;
    }
}

void stream_close_files(void)
{
    if (s_fg_fd >= 0)     { close(s_fg_fd);     s_fg_fd     = -1; }
    if (s_bg_fd >= 0)     { close(s_bg_fd);     s_bg_fd     = -1; }
    if (s_fg_row_fd >= 0) { close(s_fg_row_fd); s_fg_row_fd = -1; }
    if (s_bg_row_fd >= 0) { close(s_bg_row_fd); s_bg_row_fd = -1; }
}

uint16_t stream_get_loaded_left(void) { return s_fg_loaded_left; }
uint16_t stream_get_loaded_top(void)  { return s_fg_loaded_top; }

uint8_t stream_read_fg_tile(uint16_t wx, uint16_t wy)
{
    RIA.addr0 = (uint16_t)(FG_TILEMAP_BASE +
                            (uint16_t)((wy & RING_H_MASK) * RING_W) +
                            (uint16_t)(wx & RING_W_MASK));
    RIA.step0 = 0;
    return RIA.rw0;
}

void stream_write_fg_tile(uint16_t wx, uint16_t wy, uint8_t tile)
{
    // Write directly to XRAM ring buffer (call during VBLANK).
    RIA.addr0 = (uint16_t)(FG_TILEMAP_BASE +
                            (uint16_t)((wy & RING_H_MASK) * RING_W) +
                            (uint16_t)(wx & RING_W_MASK));
    RIA.step0 = 0;
    RIA.rw0   = tile;

    // Record cleared pickups so they persist when the ring reloads from disk.
    if (tile == 0u && s_cleared_fg_count < MAX_CLEARED_FG) {
        uint8_t i;
        for (i = 0u; i < s_cleared_fg_count; i++) {
            if (s_cleared_fg[i].wx == wx && s_cleared_fg[i].wy == wy) return; // already tracked
        }
        s_cleared_fg[s_cleared_fg_count].wx = wx;
        s_cleared_fg[s_cleared_fg_count].wy = wy;
        s_cleared_fg_count++;
    }
}
