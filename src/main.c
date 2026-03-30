#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "input.h"
#include "runningman.h"
#include "maze.h"
#include "palette.h"

unsigned RUNNING_MAN_CONFIG; // set by init_graphics(), used by runningman module
unsigned MAIN_MAP_CONFIG;    // set by init_graphics(), used by main map module


static void init_graphics(void)
{

    // Select a 320x240 canvas
    if (xreg_vga_canvas(1) < 0) {
        puts("xreg_vga_canvas failed");
        return;
    }

    RIA.addr0 = PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 256; i++) {
        RIA.rw0 = tile_palette[i] & 0xFF;
        RIA.rw0 = tile_palette[i] >> 8;
    }

    RUNNING_MAN_CONFIG = SPRITE_DATA_END; // Set the configuration address to the end of sprite data

    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, 0);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, 0);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, RUNNING_MAN_DATA);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, log_size, 4); // 16x16 pixels (2^4 = 16)
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    // Mode 4 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(4, 0, RUNNING_MAN_CONFIG, 1, 1, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    MAIN_MAP_CONFIG = RUNNING_MAN_CONFIG + sizeof(vga_mode4_sprite_t); // Place the map config right after the sprite config

    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, width_tiles,  MAIN_MAP_WIDTH_TILES);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, height_tiles, MAIN_MAP_HEIGHT_TILES);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_data_ptr,    MAIN_MAP_TILEMAP_DATA); // tile ID grid
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_palette_ptr, PALETTE_ADDR);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_tile_ptr,    MAIN_MAP_DATA);        // tile bitmaps

    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=3 (8-bit color index) => 0b0011 = 3
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x03, MAIN_MAP_CONFIG, 0, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }


}

#define SONG_HZ 60
uint8_t vsync_last = 0;
uint16_t timer_accumulator = 0;
bool music_enabled = true;

int main(void)
{

    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);

    init_graphics();
    maze_generate();
    init_input_system();
    runningman_init();

    while (true) {
        // Main game loop
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // 2. INPUT
        handle_input();

        // 3. UPDATE
        runningman_update();

        // 4. CAMERA — scroll map and reposition sprite in screen space
        {
            int16_t px = runningman_get_x();
            int16_t py = runningman_get_y();

            int16_t cam_x = px - (int16_t)SCREEN_HALF_WIDTH  + 8;
            int16_t cam_y = py - (int16_t)SCREEN_HALF_HEIGHT + 8;

            if (cam_x < 0) cam_x = 0;
            if (cam_x > (int16_t)(WORLD_W_PX - SCREEN_WIDTH))  cam_x = (int16_t)(WORLD_W_PX - SCREEN_WIDTH);
            if (cam_y < 0) cam_y = 0;
            if (cam_y > (int16_t)(WORLD_H_PX - SCREEN_HEIGHT)) cam_y = (int16_t)(WORLD_H_PX - SCREEN_HEIGHT);

            // Sprite position is relative to screen
            xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, (px - cam_x));
            xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, (py - cam_y));

            // Map scroll (negative offset shifts visible window right/down)
            xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, x_pos_px, (-cam_x));
            xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, y_pos_px, (-cam_y));
        }
    }
    return 0;
}
