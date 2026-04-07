#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "input.h"
#include "runningman.h"
#include "stream.h"
#include "enemy.h"
#include "hud.h"
#include "sound.h"

int16_t player_start_x = 80;
int16_t player_start_y = 4464;

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
#define SOLID_MAX   30u

// Ladder tiles 107-110
#define LADDER_MIN   107u
#define LADDER_MAX   110u
#define LADDER_SPEED   8    // quarter-pixels/frame climb speed

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
static bool     on_ladder;
static uint16_t ladder_col;

// ---------------------------------------------------------------------------
// Gameplay state
// ---------------------------------------------------------------------------
static uint8_t  shield_charges  = 3;
static uint8_t  shard_count     = 0;
static uint8_t  immunity_frames = 0;
static bool     terminus_collected = false;
static bool     game_over       = false;
static bool     game_won        = false;
static bool     pickup_pending  = false;
static uint16_t pickup_wx       = 0;
static uint16_t pickup_wy       = 0;

// ---------------------------------------------------------------------------
// Tile helpers
// ---------------------------------------------------------------------------

static uint8_t read_tile(uint16_t col, uint16_t row)
{
    return stream_read_fg_tile(col, row);
}

static bool tile_is_solid(uint8_t t)  { return t >= SOLID_MIN && t <= SOLID_MAX; }
static bool tile_is_ladder(uint8_t t) { return t >= LADDER_MIN && t <= LADDER_MAX; }
static bool tile_is_portal(uint8_t t) { return t >= TILE_PORTAL_MIN && t <= TILE_PORTAL_MAX; }

// Returns true if any tile overlapping the player hitbox is within [min_t, max_t].
static bool player_over_tile_range(int16_t px, int16_t py, uint8_t min_t, uint8_t max_t)
{
    uint16_t col_l = (uint16_t)((px + HITBOX_X_OFF) / TILE_W);
    uint16_t col_r = (uint16_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    uint16_t row_t = (uint16_t)(py / TILE_H);
    uint16_t row_b = (uint16_t)((py + SPRITE_H - 1) / TILE_H);
    for (uint16_t r = row_t; r <= row_b; r++) {
        for (uint16_t c = col_l; c <= col_r; c++) {
            uint8_t t = read_tile(c, r);
            if (t >= min_t && t <= max_t) return true;
        }
    }
    return false;
}

// Ladder tile overlapping the player's body (head row through sprite bottom row).
// Returns the tile column, or 0xFFFF if none.
static uint16_t ladder_col_in_body(int16_t px, int16_t py)
{
    uint16_t col_l = (uint16_t)((px + HITBOX_X_OFF) / TILE_W);
    uint16_t col_r = (uint16_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    uint16_t row_t = (uint16_t)(py / TILE_H);
    uint16_t row_b = (uint16_t)((py + SPRITE_H - 1) / TILE_H);
    for (uint16_t r = row_t; r <= row_b; r++)
        for (uint16_t c = col_l; c <= col_r; c++)
            if (tile_is_ladder(read_tile(c, r))) return c;
    return 0xFFFF;
}

// Ladder tile at the feet row (the row directly below the sprite bottom).
// Returns the tile column, or 0xFFFF if none.
static uint16_t ladder_at_feet(int16_t px, int16_t py)
{
    uint16_t row   = (uint16_t)((py + SPRITE_H) / TILE_H);
    uint16_t col_l = (uint16_t)((px + HITBOX_X_OFF) / TILE_W);
    uint16_t col_r = (uint16_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    if (tile_is_ladder(read_tile(col_l, row))) return col_l;
    if (tile_is_ladder(read_tile(col_r, row))) return col_r;
    return 0xFFFF;
}

// feet_on_solid: solid tiles OR the topmost ladder tile in a column.
// The topmost rung of every ladder acts as a one-way platform from above.
static bool feet_on_solid(int16_t px, int16_t py)
{
    uint16_t row   = (uint16_t)((py + SPRITE_H) / TILE_H);
    uint16_t col_l = (uint16_t)((px + HITBOX_X_OFF) / TILE_W);
    uint16_t col_r = (uint16_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    uint8_t tl = read_tile(col_l, row);
    uint8_t tr = read_tile(col_r, row);
    if (tile_is_solid(tl) || tile_is_solid(tr)) return true;
    // Topmost ladder tile: ladder tile with NO ladder tile directly above it
    if (tile_is_ladder(tl) && !tile_is_ladder(read_tile(col_l, row - 1))) return true;
    if (tile_is_ladder(tr) && !tile_is_ladder(read_tile(col_r, row - 1))) return true;
    return false;
}

// Solid tiles only — used during ladder descent so ladder tiles don't block climbing.
static bool feet_on_hard_ground(int16_t px, int16_t py)
{
    uint16_t row   = (uint16_t)((py + SPRITE_H) / TILE_H);
    uint16_t col_l = (uint16_t)((px + HITBOX_X_OFF) / TILE_W);
    uint16_t col_r = (uint16_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    return tile_is_solid(read_tile(col_l, row)) || tile_is_solid(read_tile(col_r, row));
}

static bool head_hits_solid(int16_t px, int16_t py)
{
    uint16_t row   = (uint16_t)(py / TILE_H);
    uint16_t col_l = (uint16_t)((px + HITBOX_X_OFF) / TILE_W);
    uint16_t col_r = (uint16_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    return tile_is_solid(read_tile(col_l, row)) || tile_is_solid(read_tile(col_r, row));
}

static bool left_wall_hit(int16_t px, int16_t py)
{
    uint16_t col   = (uint16_t)((px + HITBOX_X_OFF) / TILE_W);
    uint16_t row_t = (uint16_t)(py / TILE_H);
    uint16_t row_b = (uint16_t)((py + SPRITE_H - 1) / TILE_H);
    return tile_is_solid(read_tile(col, row_t)) || tile_is_solid(read_tile(col, row_b));
}

static bool right_wall_hit(int16_t px, int16_t py)
{
    uint16_t col   = (uint16_t)((px + HITBOX_X_OFF + HITBOX_W - 1) / TILE_W);
    uint16_t row_t = (uint16_t)(py / TILE_H);
    uint16_t row_b = (uint16_t)((py + SPRITE_H - 1) / TILE_H);
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

int16_t runningman_get_x(void) { return x_pos; }
int16_t runningman_get_y(void) { return y_pos; }

void runningman_init(void)
{
    x_pos         = player_start_x;
    x_frac        = 0;
    x_vel         = 0;
    y_pos         = player_start_y;
    y_frac        = 0;
    y_vel         = 0;
    anim_tick     = 0;
    current_frame = FRAME_IDLE_START;
    grounded      = false;
    coyote_tick   = 0;
    jump_btn_prev = false;
    last_dir      = 0;
    decel_tick    = 0;
    on_ladder       = false;
    ladder_col      = 0;
    shield_charges  = 3;
    shard_count     = 0;
    terminus_collected = false;
    game_over       = false;
    game_won        = false;
    immunity_frames = 180;  // 3 seconds of starting immunity
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, x_pos_px, x_pos);
    xram0_struct_set(RUNNING_MAN_CONFIG, vga_mode4_sprite_t, y_pos_px, y_pos);
    set_frame(FRAME_IDLE_START);
}

void runningman_update(void)
{
    if (game_over || game_won) return;

    bool up       = is_action_pressed(0, ACTION_THRUST);
    bool down     = is_action_pressed(0, ACTION_REVERSE_THRUST);
    bool left     = is_action_pressed(0, ACTION_ROTATE_LEFT);
    bool right    = is_action_pressed(0, ACTION_ROTATE_RIGHT);
    bool jump_btn = is_action_pressed(0, ACTION_FIRE);

    // -----------------------------------------------------------------------
    // Ladder engagement (UP = grab from body overlap, DOWN = enter from top)
    // -----------------------------------------------------------------------
    if (!on_ladder) {
        if (up) {
            uint16_t col = ladder_col_in_body(x_pos, y_pos);
            if (col != 0xFFFF) {
                on_ladder  = true;
                ladder_col = col;
                x_pos = (int16_t)((uint16_t)col * TILE_W) - HITBOX_X_OFF;
                x_vel = 0; x_frac = 0;
                y_vel = 0; y_frac = 0;
                grounded = false;
            }
        }
        if (!on_ladder && down && grounded) {
            uint16_t col = ladder_at_feet(x_pos, y_pos);
            if (col != 0xFFFF) {
                on_ladder  = true;
                ladder_col = col;
                x_pos = (int16_t)((uint16_t)col * TILE_W) - HITBOX_X_OFF;
                x_vel = 0; x_frac = 0;
                y_vel = 0; y_frac = 0;
                grounded = false;
            }
        }
    }

    // -----------------------------------------------------------------------
    // On-ladder: climb up/down, jump off.  No horizontal movement.
    // All exits either set grounded or let gravity take over.
    // -----------------------------------------------------------------------
    if (on_ladder) {
        if (jump_btn && !jump_btn_prev) {
            // Jump off the ladder
            on_ladder = false;
            y_vel     = -(int8_t)JUMP_VEL;
            y_frac    = 0;
            sound_play_jump();
            // fall through to normal physics this frame
        } else {
            if (up) {
                y_frac += LADDER_SPEED;
                int16_t dy = (int16_t)(y_frac >> 2);
                y_frac &= 3u;
                if (dy > 0) {
                    int16_t new_y = y_pos - dy;
                    if (new_y < 0) new_y = 0;
                    if (head_hits_solid(x_pos, new_y)) {
                        y_frac = 0; // ceiling — stop
                    } else if (ladder_col_in_body(x_pos, new_y) == 0xFFFF) {
                        // Use new_y (not the stale y_pos) so we land on the platform
                        // tile rather than one row inside the shaft.
                        // Use ceiling division to avoid truncation clipping into ceiling
                        uint16_t feet_row = (uint16_t)((new_y + SPRITE_H + TILE_H - 1) / TILE_H);
                        y_pos     = (int16_t)(feet_row * TILE_H) - SPRITE_H;
                        y_frac    = 0;
                        on_ladder = false;
                        grounded  = true;
                        // fall through to normal physics
                    } else {
                        y_pos = new_y;
                    }
                }
            } else if (down) {
                y_frac += LADDER_SPEED;
                int16_t dy = (int16_t)(y_frac >> 2);
                y_frac &= 3u;
                if (dy > 0) {
                    int16_t new_y = y_pos + dy;
                    if (new_y > WORLD_H_PX - SPRITE_H)
                        new_y = WORLD_H_PX - SPRITE_H;
                    if (feet_on_hard_ground(x_pos, new_y)) {
                        // Match normal fall collision snap so we land on top of the floor tile.
                        uint16_t row = (uint16_t)((new_y + SPRITE_H) / TILE_H);
                        y_pos     = (int16_t)(row * TILE_H) - SPRITE_H;
                        y_frac    = 0;
                        on_ladder = false;
                        grounded  = true;
                    } else if (ladder_col_in_body(x_pos, new_y) == 0xFFFF) {
                        // Past the last rung with no floor — fall
                        y_pos     = new_y;
                        on_ladder = false;
                    } else {
                        y_pos = new_y;
                    }
                }
            }
            // No input = hang still

            if (on_ladder) {
                x_vel = 0; x_frac = 0;
                if (++anim_tick >= 8u) {
                    anim_tick = 0;
                    set_frame(current_frame == 8u ? 9u : 8u);
                }
                jump_btn_prev = jump_btn;
                goto post_movement_checks; // skip physics but still process pickups/collisions
            }
            // on_ladder was cleared — fall through to normal physics
        }
    }

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
        sound_play_jump();
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
                if (nx > WORLD_W_PX - SPRITE_W) nx = WORLD_W_PX - SPRITE_W;
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
                if (ny > WORLD_H_PX - SPRITE_H) ny = WORLD_H_PX - SPRITE_H;
                if (feet_on_solid(x_pos, ny)) {
                    uint16_t row = (uint16_t)((ny + SPRITE_H) / TILE_H);
                    ny       = (int16_t)(row * TILE_H) - SPRITE_H;
                    y_vel    = 0; y_frac = 0;
                    grounded = true;
                }
                y_pos = ny;
            } else {
                int16_t ny = y_pos - dy;
                if (ny < 0) ny = 0;
                if (head_hits_solid(x_pos, ny)) {
                    uint16_t row = (uint16_t)(ny / TILE_H);
                    ny     = (int16_t)((row + 1u) * TILE_H);
                    y_vel  = 0; y_frac = 0;
                }
                y_pos = ny;
            }
        }
    }

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

post_movement_checks:
    // -----------------------------------------------------------------------
    // Pickup detection (tile under player centre)
    // -----------------------------------------------------------------------
    {
        uint16_t cx = (uint16_t)((x_pos + 8) / TILE_W);
        uint16_t cy = (uint16_t)((y_pos + 8) / TILE_H);
        uint8_t  t  = read_tile(cx, cy);
        if (t == TILE_CHARGE_PACK) {
            if (shield_charges < MAX_SHIELD_CHARGES) shield_charges++;
            hud_add_score(SCORE_CHARGE_PACK);
            sound_play_pickup_charge();
            pickup_pending = true; pickup_wx = cx; pickup_wy = cy;
        } else if (t == TILE_MEMORY_SHARD) {
            shard_count++;
            hud_add_score(SCORE_MEMORY_SHARD);
            sound_play_pickup_shard();
            pickup_pending = true; pickup_wx = cx; pickup_wy = cy;
        } else if (t == TILE_TERMINUS && !terminus_collected) {
            // Terminus is a one-time high-value prize.
            hud_add_score(SCORE_TERMINUS);
            sound_play_terminus();
            terminus_collected = true;
            pickup_pending = true; pickup_wx = cx; pickup_wy = cy;
        } else if (player_over_tile_range(x_pos, y_pos, TILE_PORTAL_MIN, TILE_PORTAL_MAX)
                   && y_pos <= 4336) {
            // Exit portal ends the run — player chooses when to leave.
            hud_add_score(SCORE_EXIT_BONUS);
            game_won = true;
        }
    }

    // -----------------------------------------------------------------------
    // Enemy collision: shield absorbs hit, enemy dies
    // -----------------------------------------------------------------------
    if (immunity_frames > 0u) {
        --immunity_frames;
    } else if (enemy_kill_overlapping_player(x_pos, y_pos)) {
        if (shield_charges > 0u) {
            shield_charges--;
            hud_add_score(-(int32_t)SCORE_SHIELD_HIT_PENALTY);
            sound_play_shield_hit();
            immunity_frames = IMMUNITY_FRAMES;
        } else {
            game_over = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Gameplay getters
// ---------------------------------------------------------------------------
uint8_t runningman_get_shield(void)   { return shield_charges; }
uint8_t runningman_get_shards(void)   { return shard_count; }
bool    runningman_is_alive(void)     { return !game_over; }
bool    runningman_is_game_won(void)  { return game_won; }

// ---------------------------------------------------------------------------
// Flush pending tile clears to XRAM (call from VBLANK only)
// ---------------------------------------------------------------------------
void runningman_flush_tile_writes(void)
{
    if (pickup_pending) {
        stream_write_fg_tile(pickup_wx, pickup_wy, TILE_EMPTY);
        pickup_pending = false;
    }
}