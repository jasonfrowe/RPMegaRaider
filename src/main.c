#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "input.h"
#include "runningman.h"
#include "stream.h"
#include "enemy.h"

// ---------------------------------------------------------------------------
// Graphics initialisation
// ---------------------------------------------------------------------------
static void init_graphics(void)
{
    // 320×240 canvas
    int rc;
    rc = xreg_vga_canvas(1);
    printf("xreg_vga_canvas(1): %d\n", rc);
    if (rc < 0) return;

    printf("BG_MODE2_CFG=0x%04X FG_MODE2_CFG=0x%04X SPRITE_CFG=0x%04X\n",
           BG_MODE2_CFG, FG_MODE2_CFG, SPRITE_CFG);
    printf("FG_TILES=0x%04X BG_TILES=0x%04X FG_TILEMAP=0x%04X BG_TILEMAP=0x%04X\n",
           FG_TILES_BASE, BG_TILES_BASE, FG_TILEMAP_BASE, BG_TILEMAP_BASE);
    printf("FG_PAL=0x%04X  BG_PAL=0x%04X\n", FG_PALETTE_BASE, BG_PALETTE_BASE);

    // Tilesets and palettes are already in XRAM (loaded from ROM at boot).

    // --- BG tile layer — plane 0 (furthest back) ---
    // OPTIONS: bit3=0 (8×8 tiles), bit[2:0]=2 (4-bit color) => 0x02
    xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, x_wrap,          true);
    xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, y_wrap,          true);
    xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, x_pos_px,        0);
    xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, y_pos_px,        0);
    xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, width_tiles,     RING_W);
    xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, height_tiles,    RING_H);
    xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, xram_data_ptr,    BG_TILEMAP_BASE);
    xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, xram_palette_ptr, BG_PALETTE_BASE);
    xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, xram_tile_ptr,    BG_TILES_BASE);

    rc = xreg_vga_mode(2, 0x02, BG_MODE2_CFG, 0, 0, 0);
    printf("xreg_vga_mode BG (plane 0): %d\n", rc);
    if (rc < 0) return;

    // --- FG tile layer — plane 1 (collision + visible terrain) ---
    xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, x_wrap,          true);
    xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, y_wrap,          true);
    xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, x_pos_px,        0);
    xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, y_pos_px,        0);
    xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, width_tiles,     RING_W);
    xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, height_tiles,    RING_H);
    xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, xram_data_ptr,    FG_TILEMAP_BASE);
    xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, xram_palette_ptr, FG_PALETTE_BASE);
    xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, xram_tile_ptr,    FG_TILES_BASE);

    rc = xreg_vga_mode(2, 0x02, FG_MODE2_CFG, 1, 0, 0);
    printf("xreg_vga_mode FG (plane 1): %d\n", rc);
    if (rc < 0) return;

    // --- Sprite layer — plane 2 (player + 7 enemies, contiguous config block) ---
    // Slot 0: player
    xram0_struct_set(SPRITE_CFG, vga_mode4_sprite_t, x_pos_px,             0);
    xram0_struct_set(SPRITE_CFG, vga_mode4_sprite_t, y_pos_px,             0);
    xram0_struct_set(SPRITE_CFG, vga_mode4_sprite_t, xram_sprite_ptr,      PLAYER_SPRITE_BASE);
    xram0_struct_set(SPRITE_CFG, vga_mode4_sprite_t, log_size,             4); // 16×16
    xram0_struct_set(SPRITE_CFG, vga_mode4_sprite_t, has_opacity_metadata, false);

    // Slots 1-7: enemies — hidden off-screen initially, positions set by enemy_init
    {
        uint8_t i;
        for (i = 1; i < SPRITE_COUNT; i++) {
            unsigned cfg = ENEMY_CFG(i);
            xram0_struct_set(cfg, vga_mode4_sprite_t, x_pos_px,             -32);
            xram0_struct_set(cfg, vga_mode4_sprite_t, y_pos_px,             -32);
            xram0_struct_set(cfg, vga_mode4_sprite_t, xram_sprite_ptr,      ENEMY_SPRITE_BASE);
            xram0_struct_set(cfg, vga_mode4_sprite_t, log_size,             4); // 16×16
            xram0_struct_set(cfg, vga_mode4_sprite_t, has_opacity_metadata, false);
        }
    }

    // Register all SPRITE_COUNT sprites with one xreg_vga_mode call.
    // LENGTH = SPRITE_COUNT covers slots 0..SPRITE_COUNT-1 from SPRITE_CFG.
    rc = xreg_vga_mode(4, 0, SPRITE_CFG, SPRITE_COUNT, 2, 0, 0);
    printf("xreg_vga_mode sprites x%u (plane 2): %d\n", (unsigned)SPRITE_COUNT, rc);
    if (rc < 0) return;

    puts("init_graphics OK");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
static uint8_t  vsync_last = 0;
static int16_t  s_cam_x    = 0;
static int16_t  s_cam_y    = 0;

// Movement debug: print position + ring state every 30 frames while moving.
static int16_t  s_prev_px    = 0;
static int16_t  s_prev_py    = 0;
static uint8_t  s_dbg_tick   = 0;

int main(void)
{
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);

    init_graphics();
    init_input_system();

    // Open maze files and pre-load the streaming ring buffer
    if (stream_open_files() < 0) return 1;

    runningman_init();   // sets player_start_x / player_start_y
    printf("player start: (%d, %d) px\n",
           (int)runningman_get_x(), (int)runningman_get_y());

    {
        // Initial camera position centred on player
        int16_t px = runningman_get_x();
        int16_t py = runningman_get_y();
        s_cam_x = px - (int16_t)SCREEN_HALF_WIDTH  + 8;
        s_cam_y = py - (int16_t)SCREEN_HALF_HEIGHT + 8;
        if (s_cam_x < 0) s_cam_x = 0;
        if (s_cam_y < 0) s_cam_y = 0;
        printf("initial cam: (%d, %d)\n", (int)s_cam_x, (int)s_cam_y);
        stream_init(s_cam_x, s_cam_y);
    }

    // Load enemy spawn positions from SPAWNS.BIN and initialise AI
    enemy_init();

    while (true) {
        // Spin-wait for vsync.  Use the idle time to read USB tile data into
        // the staging buffer (safe during active scan — no XRAM writes).
        while (RIA.vsync == vsync_last)
            stream_prefetch(s_cam_x, s_cam_y);
        vsync_last = RIA.vsync;

        // VBLANK: flush staged tile data + update all hardware registers.
        // Confining all XRAM writes to this window eliminates screen tearing.
        stream_commit();
        runningman_flush_tile_writes();        // apply any collected pickup tile clears
        enemy_draw_all(s_cam_x, s_cam_y);     // position enemy sprites in XRAM
        {
            // FG scrolls 1:1 with the camera.
            int16_t fg_x = -(int16_t)(s_cam_x % (RING_W * TILE_W));
            int16_t fg_y = -(int16_t)(s_cam_y % (RING_H * TILE_H));

            // BG scrolls at half speed — parallax depth effect.
            // Half-speed means after the camera moves 512px (one ring width),
            // the BG has only drifted 256px, wrapping seamlessly in the ring.
            int16_t bg_x = -(int16_t)((s_cam_x / 2) % (RING_W * TILE_W));
            int16_t bg_y = -(int16_t)((s_cam_y / 2) % (RING_H * TILE_H));

            xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, x_pos_px, bg_x);
            xram0_struct_set(BG_MODE2_CFG, vga_mode2_config_t, y_pos_px, bg_y);
            xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, x_pos_px, fg_x);
            xram0_struct_set(FG_MODE2_CFG, vga_mode2_config_t, y_pos_px, fg_y);
            int16_t px = runningman_get_x();
            int16_t py = runningman_get_y();
            xram0_struct_set(SPRITE_CFG, vga_mode4_sprite_t, x_pos_px, (int16_t)(px - s_cam_x));
            xram0_struct_set(SPRITE_CFG, vga_mode4_sprite_t, y_pos_px, (int16_t)(py - s_cam_y));
        }

        // ACTIVE SCAN: physics runs here — no XRAM writes.
        handle_input();
        runningman_update();
        enemy_update_all(runningman_get_x(), runningman_get_y());

        // Compute camera for next frame (used by prefetch + commit above).
        {
            int16_t px = runningman_get_x();
            int16_t py = runningman_get_y();

            // Periodic debug: print when the player is moving, once per ~30 frames.
            if (++s_dbg_tick >= 30) {
                s_dbg_tick = 0;
                if (px != s_prev_px || py != s_prev_py) {
                    printf("pos(%d,%d) cam(%d,%d) ring_l=%u ring_t=%u\n",
                           (int)px, (int)py, (int)s_cam_x, (int)s_cam_y,
                           (unsigned)stream_get_loaded_left(),
                           (unsigned)stream_get_loaded_top());
                    s_prev_px = px;
                    s_prev_py = py;
                }
            }

            s_cam_x = px - (int16_t)SCREEN_HALF_WIDTH  + 8;
            s_cam_y = py - (int16_t)SCREEN_HALF_HEIGHT + 8;
            if (s_cam_x < 0) s_cam_x = 0;
            if (s_cam_x > (int16_t)(WORLD_W_PX - SCREEN_WIDTH))  s_cam_x = (int16_t)(WORLD_W_PX - SCREEN_WIDTH);
            if (s_cam_y < 0) s_cam_y = 0;
            if (s_cam_y > (int16_t)(WORLD_H_PX - SCREEN_HEIGHT)) s_cam_y = (int16_t)(WORLD_H_PX - SCREEN_HEIGHT);
        }
    }
    return 0;
}
