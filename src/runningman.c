#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "input.h"
#include "runningman.h"

#define FRAME_SIZE      (16u * 16u * 2u)
#define SPRITE_W        16

#define FRAME_LEFT_START   0u
#define FRAME_LEFT_END     5u
#define FRAME_IDLE_START   6u
#define FRAME_IDLE_END     11u
#define FRAME_RIGHT_START  12u
#define FRAME_RIGHT_END    17u

// Velocity in quarter-pixels/frame; range -MAX_VEL..+MAX_VEL
// Pressing a direction applies ACCEL; releasing applies DECEL (friction/momentum)
#define ACCEL       1
#define DECEL       1
#define MAX_VEL     8    // 2 px/frame max speed (scaled for 16x16 sprite)
#define DECEL_RATE  4    // apply decel every Nth vsync (higher = more momentum)

// Animation ticks/frame indexed by abs_vel >> 2 (0..4); lower = faster
static const uint8_t anim_ticks_table[5] = { 15, 5, 3, 2, 1 };

// Jump arc: y offset above ground (pixels) for each of JUMP_TOTAL vsync frames.
// Advances every vsync so vertical motion is always smooth regardless of anim speed.
#define JUMP_TOTAL 36u
static const uint8_t jump_arc[JUMP_TOTAL] = {
     4,  8, 13, 18, 23, 28, 32, 36, 39, 41,
    43, 44, 45, 46, 46, 46, 46, 45, 44, 43,
    41, 39, 36, 33, 29, 25, 21, 17, 14, 11,
     8,  6,  4,  3,  2,  1
};

static int16_t  x_pos;
static uint8_t  x_frac;           // sub-pixel accumulator (0..3 quarter-pixels)
static int8_t   x_vel;            // velocity (quarter-pixels/frame)
static int16_t  ground_y;
static uint8_t  anim_tick;
static uint8_t  current_frame;
static uint8_t  jump_tick;        // vsync counter for arc lookup
static bool     jumping;
static uint8_t  jump_range_start; // animation range locked at jump start
static uint8_t  jump_range_end;
static bool     jump_btn_prev;
static int8_t   last_dir;         // -1=was going left, 1=was going right
static uint8_t  decel_tick;       // rate-limits friction for longer coast

static void set_frame(uint8_t f)
{
    unsigned ptr = RUNNING_MAN_DATA + (unsigned)f * FRAME_SIZE;
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, ptr);
    current_frame = f;
}

void runningman_init(void)
{
    x_pos            = SCREEN_HALF_WIDTH;
    x_frac           = 0;
    x_vel            = 0;
    ground_y         = SCREEN_HALF_HEIGHT;
    anim_tick        = 0;
    current_frame    = FRAME_IDLE_START;
    jumping          = false;
    jump_tick        = 0;
    jump_range_start = FRAME_IDLE_START;
    jump_range_end   = FRAME_IDLE_END;
    jump_btn_prev    = false;
    last_dir         = 0;
    decel_tick       = 0;
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, ground_y);
    set_frame(FRAME_IDLE_START);
}

void runningman_update(void)
{
    bool left     = is_action_pressed(0, ACTION_ROTATE_LEFT);
    bool right    = is_action_pressed(0, ACTION_ROTATE_RIGHT);
    bool jump_btn = is_action_pressed(0, ACTION_SUPER_FIRE);

    // Trigger jump (edge detect, grounded only); lock anim range at current velocity
    if (jump_btn && !jump_btn_prev && !jumping) {
        jumping   = true;
        jump_tick = 0;
        if (x_vel < -2) {
            jump_range_start = FRAME_LEFT_START;
            jump_range_end   = FRAME_LEFT_END;
        } else if (x_vel > 2) {
            jump_range_start = FRAME_RIGHT_START;
            jump_range_end   = FRAME_RIGHT_END;
        } else {
            jump_range_start = FRAME_IDLE_START;
            jump_range_end   = FRAME_IDLE_END;
        }
    }
    jump_btn_prev = jump_btn;

    // Accelerate / decelerate (air control unchanged for natural jump carry-through)
    if (left && !right) {
        decel_tick = 0;
        x_vel -= ACCEL;
        if (x_vel < -(int8_t)MAX_VEL) x_vel = -(int8_t)MAX_VEL;
    } else if (right && !left) {
        decel_tick = 0;
        x_vel += ACCEL;
        if (x_vel > (int8_t)MAX_VEL) x_vel = (int8_t)MAX_VEL;
    } else {
        // Friction toward zero — only applied every DECEL_RATE frames for momentum
        if (++decel_tick >= DECEL_RATE) {
            decel_tick = 0;
            if      (x_vel > 0) { x_vel -= DECEL; if (x_vel < 0) x_vel = 0; }
            else if (x_vel < 0) { x_vel += DECEL; if (x_vel > 0) x_vel = 0; }
        }
    }

    // Apply velocity to position: quarter-pixel fixed point, all 8/16-bit ops
    if (x_vel > 0) {
        x_frac += (uint8_t)x_vel;
        x_pos  += (int16_t)(x_frac >> 2);
        x_frac  &= 3u;
        if (x_pos > SCREEN_WIDTH - SPRITE_W) { x_pos = SCREEN_WIDTH - SPRITE_W; x_vel = 0; x_frac = 0; }
    } else if (x_vel < 0) {
        x_frac += (uint8_t)(-x_vel);
        x_pos  -= (int16_t)(x_frac >> 2);
        x_frac  &= 3u;
        if (x_pos < 0) { x_pos = 0; x_vel = 0; x_frac = 0; }
    }
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);

    // Smooth jump arc: update y every vsync from lookup table
    if (jumping) {
        int16_t y = ground_y - (int16_t)jump_arc[jump_tick];
        xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, y);
        if (++jump_tick >= JUMP_TOTAL) {
            jumping = false;
            xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, ground_y);
        }
    }

    // Track last significant direction (don't update while coasting through slow zone)
    if      (x_vel < -2) last_dir = -1;
    else if (x_vel >  2) last_dir =  1;

    // Animation speed from velocity magnitude (abs_vel >> 2 = table index)
    uint8_t abs_vel = (x_vel < 0) ? (uint8_t)(-x_vel) : (uint8_t)x_vel;
    uint8_t tbl_idx = abs_vel >> 2;
    if (tbl_idx > 4u) tbl_idx = 4u;
    uint8_t anim_ticks = anim_ticks_table[tbl_idx];

    // Decel frames: driven directly by abs_vel each vsync — no tick needed.
    // abs_vel==2: first decel frame (6 left / 11 right)
    // abs_vel==1: second decel frame (7 left / 10 right)
    if (!jumping && !left && !right && x_vel != 0) {
        uint8_t decel_frame = (last_dir < 0)
            ? ((abs_vel >= 2u) ? 6u : 7u)
            : ((abs_vel >= 2u) ? 11u : 10u);
        if (current_frame != decel_frame) set_frame(decel_frame);
    }

    // Tick-driven animation: run cycles, idle breathing, and jump
    if (++anim_tick >= anim_ticks) {
        anim_tick = 0;

        if (jumping) {
            if (jump_range_start == FRAME_IDLE_START) {
                // Standing jump: frame 6 ascending, frame 11 descending
                set_frame(jump_tick < JUMP_TOTAL / 2u ? FRAME_IDLE_START : FRAME_IDLE_END);
            } else {
                uint8_t r_start = jump_range_start;
                uint8_t r_end   = jump_range_end;
                if (current_frame < r_start || current_frame > r_end)
                    current_frame = r_start;
                else if (current_frame < r_end)
                    ++current_frame;
                else
                    current_frame = r_start;
                set_frame(current_frame);
            }
        } else if (left && !right) {
            // Running left: cycle frames 0-5
            if (current_frame < FRAME_LEFT_START || current_frame > FRAME_LEFT_END)
                current_frame = FRAME_LEFT_START;
            else if (current_frame < FRAME_LEFT_END)
                ++current_frame;
            else
                current_frame = FRAME_LEFT_START;
            set_frame(current_frame);
        } else if (right && !left) {
            // Running right: cycle frames 12-17
            if (current_frame < FRAME_RIGHT_START || current_frame > FRAME_RIGHT_END)
                current_frame = FRAME_RIGHT_START;
            else if (current_frame < FRAME_RIGHT_END)
                ++current_frame;
            else
                current_frame = FRAME_RIGHT_START;
            set_frame(current_frame);
        } else if (x_vel == 0) {
            // Fully stopped: alternate frames 8 and 9
            set_frame((current_frame == 8u) ? 9u : 8u);
        }
        // decel case (x_vel != 0, no input) is handled above per-vsync
    }
}