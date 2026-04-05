#ifndef ENEMY_H
#define ENEMY_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { CRAWLER = 0, FLYER = 1, TURRET = 2 } EnemyType;
typedef enum { PATROL = 0, CHASE = 1, DEAD = 2 } EnemyState;

typedef struct {
    int16_t    x;             // world pixel X (top-left of 16×16 sprite)
    int16_t    y;             // world pixel Y
    int16_t    spawn_x;       // respawn position
    int16_t    spawn_y;
    int8_t     vx;            // X velocity (CRAWLER, signed)
    uint8_t    sin_idx;       // FLYER sinusoidal phase index (0-31)
    EnemyType  type;
    EnemyState state;
    uint8_t    anim_frame;    // 0 or 1 (alternates walk/bob cycles)
    uint8_t    anim_tick;     // frame counter for animation timing
    uint8_t    sprite_idx;    // XRAM sprite slot (1-7)
    uint16_t   state_timer;   // respawn countdown while DEAD
} enemy_t;

// Load spawn positions from SPAWNS.BIN and initialise all enemy structs.
// Call after stream_init() and before the main loop.
void enemy_init(void);

// Update all enemy AI states. Call during ACTIVE SCAN (no XRAM writes).
void enemy_update_all(int16_t player_x, int16_t player_y);

// Update all enemy sprite XRAM positions. Call during VBLANK.
void enemy_draw_all(int16_t cam_x, int16_t cam_y);

// Kill every enemy whose centre is within `radius` pixels of (cx, cy).
// Killed enemies enter DEAD state and respawn after ENEMY_RESPAWN_FRAMES.
void enemy_kill_in_radius(int16_t cx, int16_t cy, uint8_t radius);

// Returns true if any live enemy overlaps the player sprite (px, py).
bool enemy_overlaps_player(int16_t px, int16_t py);

#endif // ENEMY_H
