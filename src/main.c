#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "input.h"
#include "runningman.h"

unsigned RUNNING_MAN_CONFIG; // set by init_graphics(), used by runningman module

static void init_graphics(void)
{

    // Select a 320x240 canvas
    if (xreg_vga_canvas(1) < 0) {
        puts("xreg_vga_canvas failed");
        return;
    }

    RUNNING_MAN_CONFIG = SPRITE_DATA_END; // Set the configuration address to the end of sprite data

    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, SCREEN_HALF_WIDTH);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, SCREEN_HALF_HEIGHT);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, RUNNING_MAN_DATA);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, log_size, 5); // 32x32 pixels (2^5 = 32)
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    // Mode 4 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(4, 0, RUNNING_MAN_CONFIG, 1, 1, 0, 0) < 0) {
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
    }
    return 0;
}
