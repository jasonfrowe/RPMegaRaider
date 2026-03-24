#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "input.h"
#include "runningman.h"

#define FRAME_SIZE      (32u * 32u * 2u)  // 2048 bytes per 32x32 16bpp frame
#define ANIM_TICKS      6u                // ~10 fps at 60 Hz vsync
#define SPEED           2                 // pixels per frame (ground)
#define JUMP_SPEED      4                 // pixels per frame (airborne)
#define SPRITE_W        32

// Frame ranges for each direction
#define FRAME_LEFT_START   0u
#define FRAME_LEFT_END     5u
#define FRAME_IDLE_START   6u
#define FRAME_IDLE_END     11u
#define FRAME_RIGHT_START  12u
#define FRAME_RIGHT_END    17u

// Jump arc: y offset (pixels up from ground) for each of the 6 anim steps.
// 6 steps matches one full walk cycle. All values fit in uint8_t.
#define JUMP_FRAMES 6u
static const uint8_t jump_arc[JUMP_FRAMES] = { 12, 32, 44, 44, 28, 8 };

static uint8_t  anim_tick;
static uint8_t  current_frame;
static int16_t  x_pos;
static int16_t  ground_y;
static bool     jumping;
static uint8_t  jump_index;
static bool     jump_btn_prev;

static void set_frame(uint8_t f)
{
    unsigned ptr = RUNNING_MAN_DATA + (unsigned)f * FRAME_SIZE;
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, ptr);
    current_frame = f;
}

void runningman_init(void)
{
    anim_tick     = 0;
    current_frame = FRAME_IDLE_START;
    x_pos         = SCREEN_HALF_WIDTH;
    ground_y      = SCREEN_HALF_HEIGHT;
    jumping       = false;
    jump_index    = 0;
    jump_btn_prev = false;
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, ground_y);
    set_frame(FRAME_IDLE_START);
}

void runningman_update(void)
{
    bool left     = is_action_pressed(0, ACTION_ROTATE_LEFT);
    bool right    = is_action_pressed(0, ACTION_ROTATE_RIGHT);
    bool jump_btn = is_action_pressed(0, ACTION_SUPER_FIRE);

    // Trigger jump on button-press edge, only when grounded
    if (jump_btn && !jump_btn_prev && !jumping) {
        jumping    = true;
        jump_index = 0;
    }
    jump_btn_prev = jump_btn;

    // Determine animation range and move horizontally (allowed during jump too)
    uint8_t range_start, range_end;
    uint8_t speed = jumping ? JUMP_SPEED : SPEED;
    if (left && !right) {
        range_start = FRAME_LEFT_START;
        range_end   = FRAME_LEFT_END;
        x_pos -= speed;
        if (x_pos < 0) x_pos = 0;
        xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
    } else if (right && !left) {
        range_start = FRAME_RIGHT_START;
        range_end   = FRAME_RIGHT_END;
        x_pos += speed;
        if (x_pos > SCREEN_WIDTH - SPRITE_W) x_pos = SCREEN_WIDTH - SPRITE_W;
        xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
    } else {
        range_start = FRAME_IDLE_START;
        range_end   = FRAME_IDLE_END;
    }

    // Apply arc y-offset every vsync while airborne
    if (jumping) {
        int16_t y = ground_y - (int16_t)jump_arc[jump_index];
        xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, y);
    }

    // Advance animation on tick
    if (++anim_tick >= ANIM_TICKS) {
        anim_tick = 0;

        if (jumping) {
            if (range_start == FRAME_IDLE_START) {
                // Idle jump: frame 6 on the way up, frame 11 on the way down
                set_frame(jump_index < JUMP_FRAMES / 2 ? FRAME_IDLE_START : FRAME_IDLE_END);
            } else {
                // Directional jump: walk through the directional frames once
                if (current_frame < range_start || current_frame > range_end)
                    current_frame = range_start;
                else if (current_frame < range_end)
                    ++current_frame;
                else
                    current_frame = range_start;
                set_frame(current_frame);
            }

            if (++jump_index >= JUMP_FRAMES) {
                jumping = false;
                xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, ground_y);
            }
        } else {
            // Normal ground animation
            if (current_frame < range_start || current_frame > range_end)
                current_frame = range_start;
            else if (current_frame < range_end)
                ++current_frame;
            else
                current_frame = range_start;
            set_frame(current_frame);
        }
    }
}
