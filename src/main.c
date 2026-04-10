#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "input.h"
#include "runningman.h"
#include "stream.h"
#include "enemy.h"
#include "hud.h"
#include "opl.h"
#include "sound.h"
#include "usb_hid_keys.h"

// ---------------------------------------------------------------------------
// Graphics initialisation
// ---------------------------------------------------------------------------
static void init_graphics(void)
{
    // 320×240 canvas
    int rc;
    rc = xreg_vga_canvas(1);
    if (rc < 0) return;

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
        for (i = 1; i < SHIELD_SLOT; i++) {
            unsigned cfg = ENEMY_CFG(i);
            xram0_struct_set(cfg, vga_mode4_sprite_t, x_pos_px,             -32);
            xram0_struct_set(cfg, vga_mode4_sprite_t, y_pos_px,             -32);
            xram0_struct_set(cfg, vga_mode4_sprite_t, xram_sprite_ptr,      ENEMY_SPRITE_BASE);
            xram0_struct_set(cfg, vga_mode4_sprite_t, log_size,             4); // 16×16
            xram0_struct_set(cfg, vga_mode4_sprite_t, has_opacity_metadata, false);
        }
    }

    // Slot 8: shield overlay — 32×32 ring sprite, hidden until shield is drawn
    {
        unsigned cfg = ENEMY_CFG(SHIELD_SLOT);
        xram0_struct_set(cfg, vga_mode4_sprite_t, x_pos_px,             -64);
        xram0_struct_set(cfg, vga_mode4_sprite_t, y_pos_px,             -64);
        xram0_struct_set(cfg, vga_mode4_sprite_t, xram_sprite_ptr,      SHIELD_SPRITE_BASE);
        xram0_struct_set(cfg, vga_mode4_sprite_t, log_size,             5); // 32×32
        xram0_struct_set(cfg, vga_mode4_sprite_t, has_opacity_metadata, false);
    }

    // Register all SPRITE_COUNT sprites with one xreg_vga_mode call.
    // LENGTH = SPRITE_COUNT covers slots 0..SPRITE_COUNT-1 from SPRITE_CFG.
    rc = xreg_vga_mode(4, 0, SPRITE_CFG, SPRITE_COUNT, 2, 0, 0);
    if (rc < 0) return;

    hud_init();
    OPL_Config(1, OPL_ADDR);
    opl_init();
    sound_init();
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
typedef enum { STATE_TITLE = 0, STATE_PLAYING = 1, STATE_GAMEOVER = 2 } game_state_t;

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
static uint8_t       vsync_last  = 0;
static int16_t       s_cam_x     = 0;
static int16_t       s_cam_y     = 0;
static game_state_t  s_game_state  = STATE_TITLE;
static bool          s_was_won     = false;
static bool          s_start_prev  = false;
static bool          s_alt_f4_prev = false;
// Delay before entering GAMEOVER on win so terminus fanfare can finish.
// 5 notes x 6 ticks = 30, +6 buffer = 36 frames (~0.6 s at 60 Hz).
#define WIN_FANFARE_TICKS 36u
static uint8_t       s_win_delay   = 0;

int main(void)
{
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);

    init_input_system();

    if (stream_open_files() < 0) return 1;

    runningman_init();

    {
        int16_t px = runningman_get_x();
        int16_t py = runningman_get_y();
        s_cam_x = px - (int16_t)SCREEN_HALF_WIDTH  + 8;
        s_cam_y = py - (int16_t)SCREEN_HALF_HEIGHT + 8;
        if (s_cam_x < 0) s_cam_x = 0;
        if (s_cam_y < 0) s_cam_y = 0;
        stream_init(s_cam_x, s_cam_y);
    }

    enemy_init();

    // Initialize graphics AFTER heavy stream_init XRAM writes to prevent VGA overload.
    init_graphics();

    // Start on title screen
    s_game_state = STATE_TITLE;
    hud_draw_title_screen(RIA.vsync);
    music_init(DEMO_MUSIC_FILENAME);

    while (true) {
        // Spin-wait for vsync; prefetch maze data during idle scan time.
        while (RIA.vsync == vsync_last)
            stream_prefetch(s_cam_x, s_cam_y);
        vsync_last = RIA.vsync;

        // ------------------------------------------------------------------
        // VBLANK: all XRAM writes happen here.
        // ------------------------------------------------------------------
        stream_commit();
        runningman_flush_tile_writes();
        enemy_draw_all(s_cam_x, s_cam_y);
        {
            int16_t fg_x = -(int16_t)(s_cam_x % (RING_W * TILE_W));
            int16_t fg_y = -(int16_t)(s_cam_y % (RING_H * TILE_H));
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

            // Slot 8: shield ring — 32×32, centred -8px around the 16×16 player
            {
                uint8_t sc = runningman_get_shield();
                unsigned shield_cfg = ENEMY_CFG(SHIELD_SLOT);
                if (sc > 0u && runningman_shield_is_visible()) {
                    unsigned fidx = (sc >= 3u) ? 0u : (sc == 2u) ? 1u : 2u;
                    xram0_struct_set(shield_cfg, vga_mode4_sprite_t, x_pos_px,
                                     (int16_t)((px - s_cam_x) - 8));
                    xram0_struct_set(shield_cfg, vga_mode4_sprite_t, y_pos_px,
                                     (int16_t)((py - s_cam_y) - 8));
                    xram0_struct_set(shield_cfg, vga_mode4_sprite_t, xram_sprite_ptr,
                                     (uint16_t)(SHIELD_SPRITE_BASE + fidx * SHIELD_FRAME_SIZE));
                } else {
                    xram0_struct_set(shield_cfg, vga_mode4_sprite_t, x_pos_px, -64);
                    xram0_struct_set(shield_cfg, vga_mode4_sprite_t, y_pos_px, -64);
                }
            }
        }

        // HUD text overlay (state-dependent)
        if (s_game_state == STATE_TITLE) {
            hud_draw_title_screen(RIA.vsync);
        } else if (s_game_state == STATE_PLAYING) {
            hud_draw_score();
        } else {
            hud_draw_end_screen(RIA.vsync, s_was_won);
        }

        // Audio: music sequencer + SFX (OPL writes via addr1, VBLANK-safe)
        update_music();
        sound_update(s_game_state == STATE_PLAYING
            ? enemy_get_active_type_mask(s_cam_x) : 0u);

        // ------------------------------------------------------------------
        // ACTIVE SCAN: input + physics (no XRAM writes).
        // ------------------------------------------------------------------
        handle_input();

        bool alt_down = (key(KEY_LEFTALT) != 0) || (key(KEY_RIGHTALT) != 0);
        bool alt_f4_now = alt_down && (key(KEY_F4) != 0);
        bool alt_f4_pressed = alt_f4_now && !s_alt_f4_prev;
        s_alt_f4_prev = alt_f4_now;

        if (alt_f4_pressed) {
            hud_clear();
            opl_silence_all();
            stream_close_files();
            return 0;
        }

        bool start_now = is_action_pressed(0, ACTION_PAUSE);
        bool start_pressed = start_now && !s_start_prev;
        s_start_prev = start_now;

        if (s_game_state == STATE_TITLE) {
            if (start_pressed) {
                hud_clear();
                sound_init();
                opl_silence_all();
                music_init(GAME_MUSIC_FILENAME);
                s_game_state = STATE_PLAYING;
            }
        } else if (s_game_state == STATE_PLAYING) {
            runningman_update();
            enemy_update_all(runningman_get_x(), runningman_get_y(), s_cam_x);

            if (!runningman_is_alive()) {
                s_was_won = false;
                hud_clear();
                opl_silence_all();
                s_game_state = STATE_GAMEOVER;
            } else if (runningman_is_game_won()) {
                // Delay transition so the terminus fanfare can finish.
                if (s_win_delay == 0u) {
                    s_win_delay = WIN_FANFARE_TICKS;
                } else if (--s_win_delay == 0u) {
                    s_was_won = true;
                    hud_clear();
                    opl_silence_all();
                    s_game_state = STATE_GAMEOVER;
                }
            }
        } else { // STATE_GAMEOVER
            if (start_pressed) {
                // Reset everything and return to title
                hud_reset_score();
                s_win_delay = 0;
                runningman_init();
                enemy_init();
                {
                    int16_t px = runningman_get_x();
                    int16_t py = runningman_get_y();
                    s_cam_x = px - (int16_t)SCREEN_HALF_WIDTH  + 8;
                    s_cam_y = py - (int16_t)SCREEN_HALF_HEIGHT + 8;
                    if (s_cam_x < 0) s_cam_x = 0;
                    if (s_cam_y < 0) s_cam_y = 0;
                    stream_init(s_cam_x, s_cam_y);
                }
                hud_clear();
                sound_init();
                opl_silence_all();
                music_init(DEMO_MUSIC_FILENAME);
                s_game_state = STATE_TITLE;
            }
        }

        // Camera tracks player every frame.
        {
            int16_t px = runningman_get_x();
            int16_t py = runningman_get_y();
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
