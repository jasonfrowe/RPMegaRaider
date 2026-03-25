#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "input.h"
#include "runningman.h"

#define FRAME_SIZE      (16u * 16u * 2u)
#define SPRITE_W        16
#define SPRITE_H        16
#define HITBOX_X_OFF    4
#define HITBOX_W        8
#define TILE_W          8
#define TILE_H          8

#define FRAME_LEFT_START   0u
#define FRAME_LEFT_END     5u
#define FRAME_IDLE_START   6u
#define FRAME_IDLE_END     11u
#define FRAME_RIGHT_START  12u
#define FRAME_RIGHT_END    17u

// Horizontal motion (quarter-pixels/frame)
#define ACCEL       1
#define DECEL       1
#define MAX_VEL     8
#define DECEL_RATE  3

// Vertical motion (quarter-pixels/frame)
#define GRAVITY         1
#define JUMP_VEL        20
#define MAX_FALL_VEL    20
#define COYOTE_FRAMES   2

// Solid tiles block movement
#define SOLID_MIN   1u
#define SOLID_MAX   10u

static const uint8_t anim_ticks_table[5] = { 15, 5, 3, 2, 1 };

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static int16_t  x_pos;
static uint8_t  x_frac;
static int8_t   x_vel;
static int16_t  y_pos;
static uint8_t  y_frac;
static int8_t   y_vel;
static uint8_t  anim_tick;
static uint8_t  current_frame;
static bool     grounded;
static uint8_t  coyote_tick;
static bool     jump_btn_prev;
static int8_t   last_dir;
static uint8_t  decel_tick;

// ---------------------------------------------------------------------------
// Tile helpers
// ---------------------------------------------------------------------------

static uint8_t read_tile(uint8_t col, uint8_t row)
{
    if (col >= MAIN_MAP_WIDTH_TILES || row >= MAIN_MAP_HEIGHT_TILES) return 0;
    RIA.addr0 = MAIN_MAP_TILEMAP_DATA + (unsigned)row * MAIN_MAP_WIDTH_TILES + col;
    RIA.step0 = 0;
    return RIA.rw0;
}

static bool tile_is_solid(uint8_t t) { return t >= SOLID_MIN && t <= SOLID_MAX; }

static bool feet_on_solid(int16_t px, int16_t py)
{
    uint8_t row   = (uint8_t)((py + SPRITE_H) / TILE_H);
    uint8_t col_l = (uint8_t)((px + HITBOX_X_OFF) / TILE_W);
    uint8_t col_r = (uint8_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    return tile_is_solid(read_tile(col_l, row)) || tile_is_solid(read_tile(col_r, row));
}

static bool head_hits_solid(int16_t px, int16_t py)
{
    uint8_t row   = (uint8_t)(py / TILE_H);
    uint8_t col_l = (uint8_t)((px + HITBOX_X_OFF) / TILE_W);
    uint8_t col_r = (uint8_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    return tile_is_solid(read_tile(col_l, row)) || tile_is_solid(read_tile(col_r, row));
}

static bool left_wall_hit(int16_t px, int16_t py)
{
    uint8_t col   = (uint8_t)((px + HITBOX_X_OFF) / TILE_W);
    uint8_t row_t = (uint8_t)(py / TILE_H);
    uint8_t row_b = (uint8_t)((py + SPRITE_H - 1) / TILE_H);
    return tile_is_solid(read_tile(col, row_t)) || tile_is_solid(read_tile(col, row_b));
}

static bool right_wall_hit(int16_t px, int16_t py)
{
    uint8_t col   = (uint8_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    uint8_t row_t = (uint8_t)(py / TILE_H);
    uint8_t row_b = (uint8_t)((py + SPRITE_H - 1) / TILE_H);
    return tile_is_solid(read_tile(col, row_t)) || tile_is_solid(read_tile(col, row_b));
}

// ---------------------------------------------------------------------------
// Frame helper
// ---------------------------------------------------------------------------

static void set_frame(uint8_t f)
{
    unsigned ptr = RUNNING_MAN_DATA + (unsigned)f * FRAME_SIZE;
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, ptr);
    current_frame = f;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void runningman_init(void)
{
    x_pos         = PLAYER_START_X;
    x_frac        = 0;
    x_vel         = 0;
    y_pos         = PLAYER_START_Y;
    y_frac        = 0;
    y_vel         = 0;
    anim_tick     = 0;
    current_frame = FRAME_IDLE_START;
    grounded      = false;
    coyote_tick   = 0;
    jump_btn_prev = false;
    last_dir      = 0;
    decel_tick    = 0;
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, y_pos);
    set_frame(FRAME_IDLE_START);
}

void runningman_update(void)
{
    bool left     = is_action_pressed(0, ACTION_ROTATE_LEFT);
    bool right    = is_action_pressed(0, ACTION_ROTATE_RIGHT);
    bool jump_btn = is_action_pressed(0, ACTION_FIRE);

    // -----------------------------------------------------------------------
    // Vertical physics: gravity, coyote time, jump
    // -----------------------------------------------------------------------
    if (grounded && !feet_on_solid(x_pos, y_pos)) {
        grounded    = false;
        coyote_tick = COYOTE_FRAMES;
    }

    if (jump_btn && !jump_btn_prev && (grounded || coyote_tick > 0)) {
        y_vel       = -(int8_t)JUMP_VEL;
        grounded    = false;
        coyote_tick = 0;
    }
    jump_btn_prev = jump_btn;

    if (coyote_tick > 0) --coyote_tick;

    if (!grounded) {
        y_vel += GRAVITY;
        if (y_vel > (int8_t)MAX_FALL_VEL) y_vel = (int8_t)MAX_FALL_VEL;
    }

    // -----------------------------------------------------------------------
    // Horizontal input
    // -----------------------------------------------------------------------
    if (left && !right) {
        decel_tick = 0;
        x_vel -= ACCEL;
        if (x_vel < -(int8_t)MAX_VEL) x_vel = -(int8_t)MAX_VEL;
    } else if (right && !left) {
        decel_tick = 0;
        x_vel += ACCEL;
        if (x_vel > (int8_t)MAX_VEL) x_vel = (int8_t)MAX_VEL;
    } else {
        if (++decel_tick >= DECEL_RATE) {
            decel_tick = 0;
            if      (x_vel > 0) { x_vel -= DECEL; if (x_vel < 0) x_vel = 0; }
            else if (x_vel < 0) { x_vel += DECEL; if (x_vel > 0) x_vel = 0; }
        }
    }

    // -----------------------------------------------------------------------
    // Apply X with wall collision
    // -----------------------------------------------------------------------
    if (x_vel != 0) {
        uint8_t step = (x_vel < 0) ? (uint8_t)(-x_vel) : (uint8_t)x_vel;
        x_frac += step;
        int16_t dx = (int16_t)(x_frac >> 2);
        x_frac &= 3u;
        if (dx > 0) {
            if (x_vel > 0) {
                int16_t nx = x_pos + dx;
                if (nx > SCREEN_WIDTH - SPRITE_W) nx = SCREEN_WIDTH - SPRITE_W;
                if (right_wall_hit(nx, y_pos)) { nx = x_pos; x_vel = 0; x_frac = 0; }
                x_pos = nx;
            } else {
                int16_t nx = x_pos - dx;
                if (nx < 0) nx = 0;
                if (left_wall_hit(nx, y_pos)) { nx = x_pos; x_vel = 0; x_frac = 0; }
                x_pos = nx;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Apply Y with floor/ceiling collision
    // -----------------------------------------------------------------------
    if (y_vel != 0) {
        uint8_t step = (y_vel < 0) ? (uint8_t)(-y_vel) : (uint8_t)y_vel;
        y_frac += step;
        int16_t dy = (int16_t)(y_frac >> 2);
        y_frac &= 3u;
        if (dy > 0) {
            if (y_vel > 0) {
                int16_t ny = y_pos + dy;
                if (ny > SCREEN_HEIGHT - SPRITE_H) ny = SCREEN_HEIGHT - SPRITE_H;
                if (feet_on_solid(x_pos, ny)) {
                    uint8_t row = (uint8_t)((ny + SPRITE_H) / TILE_H);
                    ny       = (int16_t)(row * TILE_H) - SPRITE_H;
                    y_vel    = 0; y_frac = 0;
                    grounded = true;
                }
                y_pos = ny;
            } else {
                int16_t ny = y_pos - dy;
                if (ny < 0) ny = 0;
                if (head_hits_solid(x_pos, ny)) {
                    uint8_t row = (uint8_t)(ny / TILE_H);
                    ny     = (int16_t)((row + 1u) * TILE_H);
                    y_vel  = 0; y_frac = 0;
                }
                y_pos = ny;
            }
        }
    }

    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, y_pos);

    // -----------------------------------------------------------------------
    // Animation
    // -----------------------------------------------------------------------
    bool airborne = !grounded;

    if      (x_vel < -2) last_dir = -1;
    else if (x_vel >  2) last_dir =  1;

    uint8_t abs_vel = (x_vel < 0) ? (uint8_t)(-x_vel) : (uint8_t)x_vel;
    uint8_t tbl_idx = abs_vel >> 2;
    if (tbl_idx > 4u) tbl_idx = 4u;
    uint8_t anim_ticks = anim_ticks_table[tbl_idx];

    if (grounded && !left && !right && x_vel != 0) {
        uint8_t decel_frame = (last_dir < 0)
            ? ((abs_vel >= 2u) ? 6u : 7u)
            : ((abs_vel >= 2u) ? 11u : 10u);
        if (current_frame != decel_frame) set_frame(decel_frame);
    }

    if (++anim_tick >= anim_ticks) {
        anim_tick = 0;

        if (airborne) {
            uint8_t r_start = (last_dir < 0) ? FRAME_LEFT_START  : FRAME_RIGHT_START;
            uint8_t r_end   = (last_dir < 0) ? FRAME_LEFT_END    : FRAME_RIGHT_END;
            if (current_frame < r_start || current_frame > r_end) current_frame = r_start;
            else if (current_frame < r_end)                        ++current_frame;
            else                                                    current_frame = r_start;
            set_frame(current_frame);
        } else if (left && !right) {
            if (current_frame < FRAME_LEFT_START || current_frame > FRAME_LEFT_END) current_frame = FRAME_LEFT_START;
            else if (current_frame < FRAME_LEFT_END) ++current_frame;
            else current_frame = FRAME_LEFT_START;
            set_frame(current_frame);
        } else if (right && !left) {
            if (current_frame < FRAME_RIGHT_START || current_frame > FRAME_RIGHT_END) current_frame = FRAME_RIGHT_START;
            else if (current_frame < FRAME_RIGHT_END) ++current_frame;
            else current_frame = FRAME_RIGHT_START;
            set_frame(current_frame);
        } else if (x_vel == 0 && !airborne) {
            set_frame((current_frame == 8u) ? 9u : 8u);
        }
    }
}