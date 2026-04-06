#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "constants.h"
#include "stream.h"
#include "enemy.h"

// ---------------------------------------------------------------------------
// Sprite data frame layout in ENEMY_SPRITE_BASE:
//   Frame 0 — Crawler walk A  (bytes 0x0000..0x01FF)
//   Frame 1 — Crawler walk B  (bytes 0x0200..0x03FF)
//   Frame 2 — Flyer bob A     (bytes 0x0400..0x05FF)
//   Frame 3 — Flyer bob B     (bytes 0x0600..0x07FF)
//   Frame 4 — Turret idle     (bytes 0x0800..0x09FF)
//   Frame 5 — Turret armed    (bytes 0x0A00..0x0BFF)
// ---------------------------------------------------------------------------
#define ENEMY_FRAME_BYTES  512u    // 16×16 pixels × 2 bytes/pixel

static unsigned frame_ptr(EnemyType type, uint8_t anim_frame)
{
    uint8_t base;
    switch (type) {
        case RUSHER:  base = (uint8_t)(0u + (anim_frame & 1u)); break;
        case TRACKER: base = (uint8_t)(2u + (anim_frame & 1u)); break;
        case GHOST:   base = (uint8_t)(4u + (anim_frame & 1u)); break;
        default:      base = 0u; break;
    }
    return ENEMY_SPRITE_BASE + (unsigned)base * ENEMY_FRAME_BYTES;
}

// ---------------------------------------------------------------------------
// Enemy pool
// ---------------------------------------------------------------------------
static enemy_t s_enemies[MAX_ENEMIES];
static uint8_t s_num_enemies = 0;

// ---------------------------------------------------------------------------
// Breadcrumb trail — Type 2 (TRACKER)
// Player position is sampled every CRUMB_INTERVAL frames into a circular
// buffer.  TRACKERs follow the trail from the oldest crumb to the newest.
// ---------------------------------------------------------------------------
#define CRUMB_COUNT    16u
#define CRUMB_INTERVAL 25u

static int16_t s_crumb_x[CRUMB_COUNT];
static int16_t s_crumb_y[CRUMB_COUNT];
static uint8_t s_crumb_head = 0;    // next write slot
static uint8_t s_crumb_fill = 0;    // valid crumbs in buffer (0..CRUMB_COUNT)
static uint8_t s_crumb_tick = 0;

// Last known horizontal direction of the player — GHOST uses this for intercept.
static int8_t  s_player_dir    =  1;
static int16_t s_player_prev_x =  0;

static void record_crumb(int16_t px, int16_t py)
{
    if      (px > s_player_prev_x) s_player_dir =  1;
    else if (px < s_player_prev_x) s_player_dir = -1;
    s_player_prev_x = px;

    if (++s_crumb_tick < CRUMB_INTERVAL) return;
    s_crumb_tick = 0;
    s_crumb_x[s_crumb_head] = px;
    s_crumb_y[s_crumb_head] = py;
    s_crumb_head = (uint8_t)((s_crumb_head + 1u) & (CRUMB_COUNT - 1u));
    if (s_crumb_fill < CRUMB_COUNT) s_crumb_fill++;
}

// Respawn an enemy just off-screen near the player.
// Left/right side alternates by sprite_idx so enemies spread out.
static void respawn_offscreen(enemy_t *e, int16_t px, int16_t py, int16_t cam_x)
{
    // Spawn ahead of the player's movement direction so they are forced into an encounter
    uint8_t side;
    if (s_player_dir > 0)      side = 1; // Player moving right, spawn right
    else if (s_player_dir < 0) side = 0; // Player moving left, spawn left
    else                       side = (uint8_t)(e->sprite_idx & 1u);

    // Stagger multiple enemies spawning at the same time so they don't perfectly overlap
    int16_t offset = (int16_t)((e->sprite_idx & 3u) * 32);

    int16_t  spawn_x = side
        ? (int16_t)(cam_x + (int16_t)(SCREEN_WIDTH  + 24 + offset))
        : (int16_t)(cam_x - 24 - offset);

    if (spawn_x < 16) spawn_x = 16;
    if (spawn_x > (int16_t)(WORLD_W_PX - 16)) spawn_x = (int16_t)(WORLD_W_PX - 16);

    e->x = spawn_x;

    if (e->type == GHOST) {
        // Snap to the nearest 8-pixel floor grid based on the player's height.
        e->y = (int16_t)(py & ~7);
    } else {
        e->y = py;       // always at the player's current height
    }

    e->spawn_x     = e->x;
    e->spawn_y     = e->y;
    e->state       = PATROL;
    e->vx          = side ? (int8_t)-3 : (int8_t)3;  // heading toward player
    e->anim_frame  = 0u;
    e->anim_tick   = 0u;
    e->state_timer = 0u;

    // Re-initialise TRACKER crumb index to the oldest crumb so it re-traces the trail.
    if (e->type == TRACKER) {
        uint8_t tail = (uint8_t)((s_crumb_head + CRUMB_COUNT - s_crumb_fill) & (CRUMB_COUNT - 1u));
        e->crumb_idx = tail;
    }
}

// ---------------------------------------------------------------------------
// Per-type update functions
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Type 1 — RUSHER
// Fast horizontal sweeper. Sweeps straight across the screen so the player 
// can predict it and jump over it. Exits the screen edge → DEAD with a short delay.
// ---------------------------------------------------------------------------
static void update_rusher(enemy_t *e, int16_t px, int16_t py, int16_t cam_x)
{
    (void)px; (void)py;
    int16_t screen_x = (int16_t)(e->x - cam_x);

    // Walked off-screen → enter DEAD with a short respawn delay.
    // Check if fully off screen, using a wide margin (150px) to comfortably fit the 
    // stagger offset from fresh off-screen spawns so they don't instantly die.
    if (screen_x < -150 || screen_x > (int16_t)(SCREEN_WIDTH + 150)) {
        e->state       = DEAD;
        e->state_timer = 120u;   // ~2 s before reappearing
        return;
    }

    // Ensure a non-zero speed.
    if (e->vx == 0) e->vx = (int8_t)3;

    e->x = (int16_t)(e->x + e->vx);

    // Hard world-edge guard.
    if (e->x < 16)                          { e->x = 16;                             e->vx =  (int8_t)3; }
    if (e->x > (int16_t)(WORLD_W_PX - 16)) { e->x = (int16_t)(WORLD_W_PX - 16);     e->vx = (int8_t)-3; }

    e->state = CHASE;
    if (++e->anim_tick >= 4u) { e->anim_tick = 0u; e->anim_frame ^= 1u; }
}

// ---------------------------------------------------------------------------
// Type 2 — TRACKER
// Follows the player's breadcrumb trail at speed 1 (slower than the player).
// Retraces the player's actual path including vertical movement.
// Large gaps in the trail (jumps / fast scroll) are skipped.
// ---------------------------------------------------------------------------
static void update_tracker(enemy_t *e, int16_t px, int16_t py)
{
    // No crumbs recorded yet — drift very slowly toward the player.
    if (s_crumb_fill == 0) {
        int16_t dx = (int16_t)(px - e->x);
        int16_t dy = (int16_t)(py - e->y);
        
        int16_t step = (e->anim_tick & 1u) ? 2 : 1;
        if (dx >  8) e->x = (int16_t)(e->x + step);
        else if (dx < -8) e->x = (int16_t)(e->x - step);
        if (dy >  8) e->y = (int16_t)(e->y + step);
        else if (dy < -8) e->y = (int16_t)(e->y - step);
        
        e->state = PATROL;
        if (++e->anim_tick >= 12u) { e->anim_tick = 0u; e->anim_frame ^= 1u; }
        return;
    }

    // Move toward the current target crumb.
    int16_t tx  = s_crumb_x[e->crumb_idx];
    int16_t ty  = s_crumb_y[e->crumb_idx];
    int16_t dx  = (int16_t)(tx - e->x);
    int16_t dy  = (int16_t)(ty - e->y);
    int16_t adx = (dx < 0) ? (int16_t)-dx : dx;
    int16_t ady = (dy < 0) ? (int16_t)-dy : dy;

    // Smooth ~0.75x motion: alternate 1 and 2 pixels per frame (averages 1.5)
    int16_t step = (e->anim_tick & 1u) ? 2 : 1;
    if (adx > 6) e->x = (int16_t)(e->x + (dx > 0 ? step : -step));
    if (ady > 6) e->y = (int16_t)(e->y + (dy > 0 ? step : -step));

    // Reached this crumb — advance toward the most recent one.
    if (adx < 16 && ady < 16) {
        uint8_t next = (uint8_t)((e->crumb_idx + 1u) & (CRUMB_COUNT - 1u));
        if (next != s_crumb_head) {
            // Large gap between consecutive crumbs = player jumped; skip past it.
            int16_t gx = (int16_t)(s_crumb_x[next] - s_crumb_x[e->crumb_idx]);
            int16_t gy = (int16_t)(s_crumb_y[next] - s_crumb_y[e->crumb_idx]);
            if (gx < 0) gx = (int16_t)-gx;
            if (gy < 0) gy = (int16_t)-gy;
            e->crumb_idx = next;
            if (gx > 80 || gy > 80) {
                uint8_t skip = (uint8_t)((next + 1u) & (CRUMB_COUNT - 1u));
                if (skip != s_crumb_head) e->crumb_idx = skip;
            }
        }
    }

    // World bounds.
    if (e->x < 16) e->x = 16;
    if (e->x > (int16_t)(WORLD_W_PX - 16)) e->x = (int16_t)(WORLD_W_PX - 16);
    if (e->y < 8)  e->y = 8;
    if (e->y > (int16_t)(WORLD_H_PX - 16)) e->y = (int16_t)(WORLD_H_PX - 16);

    e->state = CHASE;
    if (++e->anim_tick >= 10u) { e->anim_tick = 0u; e->anim_frame ^= 1u; }
}

// ---------------------------------------------------------------------------
// Type 3 — GHOST (Ms. Pac-Man intercept targeting)
// Normally aims 48 px ahead of the player in their last-known direction.
// When the player is near a pickup or goal tile, the ghost speeds up and
// switches to direct pursuit.  Floats freely in 2-D.
// ---------------------------------------------------------------------------
static void update_ghost(enemy_t *e, int16_t px, int16_t py)
{
    // Scan a 5×5-tile window around the player for pickup / goal tiles.
    bool triggered = false;
    {
        int16_t pdx = (int16_t)(px - e->x); if (pdx < 0) pdx = (int16_t)-pdx;
        int16_t pdy = (int16_t)(py - e->y); if (pdy < 0) pdy = (int16_t)-pdy;
        if (pdx < 200 && pdy < 200) {
            uint16_t ring_left = stream_get_loaded_left();
            uint16_t ring_top  = stream_get_loaded_top();
            uint16_t pcx = (uint16_t)((uint16_t)(px + 8) / TILE_W);
            uint16_t pcy = (uint16_t)((uint16_t)(py + 8) / TILE_H);
            uint16_t r, c;
            for (r = (pcy > 2u ? pcy - 2u : 0u); r <= pcy + 2u && !triggered; r++) {
                if (r < ring_top || r >= ring_top + RING_H) continue;
                for (c = (pcx > 2u ? pcx - 2u : 0u); c <= pcx + 2u && !triggered; c++) {
                    if (c < ring_left || c >= ring_left + RING_W) continue;
                    uint8_t t = stream_read_fg_tile(c, r);
                    if (t == TILE_CHARGE_PACK || t == TILE_MEMORY_SHARD || t == TILE_TERMINUS)
                        triggered = true;
                }
            }
        }
    }

    // Target: intercept ahead of player (normal) or direct pursuit (triggered).
    int16_t tx = triggered
        ? px
        : (int16_t)(px + (int16_t)(s_player_dir * 48));

    // ONLY track horizontally to stay on its designated floor tile!
    int16_t dx  = (int16_t)(tx - e->x);
    int16_t adx = (dx < 0) ? (int16_t)-dx : dx;

    // Smooth ~0.75x player speed: accumulate 3 quarter-px/frame, move 1px per 4 qpx.
    // Normal: 3 qpx/frame → 0.75 px/frame (player max = 2 px/frame → 0.375x).
    // Triggered: 6 qpx/frame → 1.5 px/frame (0.75x player).
    uint8_t qspeed = triggered ? 6u : 3u;
    if (adx > 4) {
        e->crumb_idx += qspeed;  // reuse crumb_idx as fractional accumulator
        int16_t step = (int16_t)(e->crumb_idx >> 2);
        e->crumb_idx &= 3u;
        if (step > 0)
            e->x = (int16_t)(e->x + (dx > 0 ? step : -step));
    } else {
        e->crumb_idx = 0u;
    }

    // World bounds (only X is checked since Y doesn't change anymore).
    if (e->x < 16) e->x = 16;
    if (e->x > (int16_t)(WORLD_W_PX - 16)) e->x = (int16_t)(WORLD_W_PX - 16);

    e->state = triggered ? CHASE : PATROL;
    e->anim_frame = triggered ? 1u : 0u; // 0 = idle (red), 1 = armed (yellow spark)
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void enemy_init(void)
{
    int fd = open("ROM:SPAWNS.BIN", O_RDONLY);
    if (fd < 0) {
        puts("enemy: SPAWNS.BIN not found — no enemies");
        s_num_enemies = 0;
        return;
    }

    uint8_t count = 0;
    read(fd, &count, 1);
    if (count > MAX_ENEMIES) count = MAX_ENEMIES;

    uint8_t i;
    for (i = 0; i < count; i++) {
        uint16_t x_px = 0, y_px = 0;
        uint8_t  typ  = 0;
        read(fd, &x_px, 2);
        read(fd, &y_px, 2);
        read(fd, &typ,  1);

        enemy_t *e = &s_enemies[i];
        e->x         = (int16_t)x_px;
        e->y         = (int16_t)y_px;
        e->spawn_x   = (int16_t)x_px;
        e->spawn_y   = (int16_t)y_px;
        e->type      = (EnemyType)(typ <= 2u ? typ : 0u);
        e->state     = DEAD; // Start dead so they wait until grace period ends
        e->vx        = (e->type == RUSHER) ? ((i & 1u) ? (int8_t)3 : (int8_t)-3) : (int8_t)0;
        e->anim_frame = 0;
        e->anim_tick  = (uint8_t)(i * 7u); // stagger animation timings
        e->sprite_idx = (uint8_t)(i + 1u); // slots 1-7; slot 0 = player
        e->state_timer = (uint16_t)(i * 30u); // Stagger their spawns
        e->crumb_idx = 0;
        e->activated = 0;
    }
    close(fd);
    s_num_enemies = count;

    printf("enemy: %u enemies loaded\n", (unsigned)s_num_enemies);
}

void enemy_update_all(int16_t player_x, int16_t player_y, int16_t cam_x)
{
    record_crumb(player_x, player_y);

    uint8_t i;
    for (i = 0; i < s_num_enemies; i++) {
        enemy_t *e = &s_enemies[i];

        if (!e->activated) {
            if (e->type == RUSHER  && player_y <= 4336) e->activated = 1;
            if (e->type == TRACKER && player_y <= 4264) e->activated = 1;
            if (e->type == GHOST   && player_y <= 4152) e->activated = 1;
            
            if (!e->activated) continue;
        }

        // Dead: count down, then respawn just off-screen.
        if (e->state == DEAD) {
            if (e->state_timer > 0u) e->state_timer--;
            else                     respawn_offscreen(e, player_x, player_y, cam_x);
            continue;
        }

        // Any live enemy too far from the player gets repositioned off-screen.
        // This also fires on the very first frame for enemies spawned far away,
        // so all enemies appear near the player from the start.
        {
            int16_t dx = (int16_t)(e->x - player_x); if (dx < 0) dx = (int16_t)-dx;
            int16_t dy = (int16_t)(e->y - player_y); if (dy < 0) dy = (int16_t)-dy;
            
            if (dx > 460 || dy > 360) {
                respawn_offscreen(e, player_x, player_y, cam_x);
                continue;
            }
        }

        switch (e->type) {
            case RUSHER:  update_rusher (e, player_x, player_y, cam_x); break;
            case TRACKER: update_tracker(e, player_x, player_y);        break;
            case GHOST:   update_ghost  (e, player_x, player_y);        break;
        }
    }
}

void enemy_draw_all(int16_t cam_x, int16_t cam_y)
{
    uint8_t i;
    for (i = 0; i < s_num_enemies; i++) {
        enemy_t *e = &s_enemies[i];
        unsigned cfg = ENEMY_CFG(e->sprite_idx);

        if (e->state == DEAD) {
            xram0_struct_set(cfg, vga_mode4_sprite_t, x_pos_px, -32);
            xram0_struct_set(cfg, vga_mode4_sprite_t, y_pos_px, -32);
            continue;
        }

        int16_t screen_x = (int16_t)(e->x - cam_x);
        int16_t screen_y = (int16_t)(e->y - cam_y);
        xram0_struct_set(cfg, vga_mode4_sprite_t, x_pos_px, screen_x);
        xram0_struct_set(cfg, vga_mode4_sprite_t, y_pos_px, screen_y);
        xram0_struct_set(cfg, vga_mode4_sprite_t, xram_sprite_ptr,
                         frame_ptr(e->type, e->anim_frame));
    }
}

void enemy_kill_in_radius(int16_t cx, int16_t cy, uint8_t radius)
{
    // Use uint32_t for r2 — radius=180 gives r2=32400 which overflows uint16_t.
    uint32_t r2 = (uint32_t)radius * (uint32_t)radius;
    uint8_t i;
    for (i = 0; i < s_num_enemies; i++) {
        enemy_t *e = &s_enemies[i];
        if (e->state == DEAD) continue;

        // Quick bounding-box reject
        int16_t dx = (int16_t)((e->x + 8) - cx);
        int16_t dy = (int16_t)((e->y + 8) - cy);
        if (dx > (int16_t)radius || dx < -(int16_t)radius) continue;
        if (dy > (int16_t)radius || dy < -(int16_t)radius) continue;

        uint32_t d2 = (uint32_t)((int32_t)dx * dx) + (uint32_t)((int32_t)dy * dy);
        if (d2 <= r2) {
            e->state       = DEAD;
            e->state_timer = ENEMY_RESPAWN_FRAMES;
            printf("enemy %u killed by EMP\n", (unsigned)i);
        }
    }
}

bool enemy_overlaps_player(int16_t px, int16_t py)
{
    uint8_t i;
    for (i = 0; i < s_num_enemies; i++) {
        enemy_t *e = &s_enemies[i];
        if (e->state == DEAD) continue;

        // 10px proximity check on sprite centres
        int16_t dx = (int16_t)((e->x + 8) - (px + 8));
        int16_t dy = (int16_t)((e->y + 8) - (py + 8));
        if (dx < 0) dx = (int16_t)-dx;
        if (dy < 0) dy = (int16_t)-dy;
        if (dx < 10 && dy < 10) return true;
    }
    return false;
}

bool enemy_kill_overlapping_player(int16_t px, int16_t py)
{
    uint8_t i;
    for (i = 0; i < s_num_enemies; i++) {
        enemy_t *e = &s_enemies[i];
        if (e->state == DEAD) continue;

        int16_t dx = (int16_t)((e->x + 8) - (px + 8));
        int16_t dy = (int16_t)((e->y + 8) - (py + 8));
        if (dx < 0) dx = (int16_t)-dx;
        if (dy < 0) dy = (int16_t)-dy;
        if (dx < 12 && dy < 12) {
            e->state       = DEAD;
            e->state_timer = ENEMY_RESPAWN_FRAMES;
            printf("enemy %u killed by shield contact\n", (unsigned)i);
            return true;
        }
    }
    return false;
}

uint8_t enemy_get_active_type_mask(int16_t cam_x)
{
    uint8_t mask = 0;
    int16_t left  = (int16_t)(cam_x - 32);
    int16_t right = (int16_t)(cam_x + (int16_t)SCREEN_WIDTH + 32);
    uint8_t i;
    for (i = 0; i < s_num_enemies; i++) {
        enemy_t *e = &s_enemies[i];
        if (e->state == DEAD) continue;
        if (e->x < left || e->x > right) continue;
        switch (e->type) {
            case RUSHER:  mask |= 0x01u; break;
            case TRACKER: mask |= 0x02u; break;
            case GHOST:   mask |= 0x04u; break;
        }
    }
    return mask;
}
