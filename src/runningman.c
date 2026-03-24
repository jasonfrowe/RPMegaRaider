#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "input.h"
#include "runningman.h"

#define FRAME_SIZE      (32u * 32u * 2u)  // 2048 bytes per 32x32 16bpp frame
#define ANIM_TICKS      6u                // ~10 fps at 60 Hz vsync
#define SPEED           2                 // pixels per frame
#define SPRITE_W        32

// Frame ranges for each direction
#define FRAME_LEFT_START   0u
#define FRAME_LEFT_END     5u
#define FRAME_IDLE_START   6u
#define FRAME_IDLE_END     11u
#define FRAME_RIGHT_START  12u
#define FRAME_RIGHT_END    17u

static uint8_t  anim_tick;
static uint8_t  current_frame;
static int16_t  x_pos;

void runningman_init(void)
{
    anim_tick     = 0;
    current_frame = FRAME_IDLE_START;
    x_pos         = SCREEN_HALF_WIDTH;
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
}

void runningman_update(void)
{
    bool left  = is_action_pressed(0, ACTION_ROTATE_LEFT);
    bool right = is_action_pressed(0, ACTION_ROTATE_RIGHT);

    // Determine target frame range and move
    uint8_t range_start, range_end;
    if (left && !right) {
        range_start = FRAME_LEFT_START;
        range_end   = FRAME_LEFT_END;
        x_pos -= SPEED;
        if (x_pos < 0) x_pos = 0;
        xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
    } else if (right && !left) {
        range_start = FRAME_RIGHT_START;
        range_end   = FRAME_RIGHT_END;
        x_pos += SPEED;
        if (x_pos > SCREEN_WIDTH - SPRITE_W) x_pos = SCREEN_WIDTH - SPRITE_W;
        xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
    } else {
        range_start = FRAME_IDLE_START;
        range_end   = FRAME_IDLE_END;
    }

    // If the current frame is outside the new range, jump to start of range
    if (current_frame < range_start || current_frame > range_end)
        current_frame = range_start;

    // Advance animation
    if (++anim_tick >= ANIM_TICKS) {
        anim_tick = 0;
        if (current_frame >= range_end)
            current_frame = range_start;
        else
            ++current_frame;

        unsigned ptr = RUNNING_MAN_DATA + (unsigned)current_frame * FRAME_SIZE;
        xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, ptr);
    }
}
