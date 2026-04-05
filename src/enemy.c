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
        case CRAWLER: base = (uint8_t)(0u + (anim_frame & 1u)); break;
        case FLYER:   base = (uint8_t)(2u + (anim_frame & 1u)); break;
        case TURRET:  base = (uint8_t)(4u + (anim_frame & 1u)); break;
        default:      base = 0u; break;
    }
    return ENEMY_SPRITE_BASE + (unsigned)base * ENEMY_FRAME_BYTES;
}

// ---------------------------------------------------------------------------
// Simple 16-element sine approximation for flyer bob.
// Maps phase_idx (0-31) to ±3 pixel vertical displacement.
// ---------------------------------------------------------------------------
static const int8_t s_sin16[16] = {
    0, 1, 2, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1
};

static int8_t bob_y(uint8_t phase)
{
    return s_sin16[phase & 15u];
}

// ---------------------------------------------------------------------------
// Enemy pool
// ---------------------------------------------------------------------------
static enemy_t s_enemies[MAX_ENEMIES];
static uint8_t s_num_enemies = 0;

// ---------------------------------------------------------------------------
// AI helpers — tile queries via ring buffer
// ---------------------------------------------------------------------------

// Returns true when world tile (col, row) is a solid platform/wall tile
// AND the tile is currently within the ring buffer's loaded window.
static bool solid_at(uint16_t col, uint16_t row)
{
    uint16_t left = stream_get_loaded_left();
    uint16_t top  = stream_get_loaded_top();
    if (col < left || col >= left + RING_W) return false;
    if (row < top  || row >= top  + RING_H) return false;
    uint8_t t = stream_read_fg_tile(col, row);
    return (t >= 1u && t <= 30u);
}

// ---------------------------------------------------------------------------
// Per-type update functions
// ---------------------------------------------------------------------------

static void update_crawler(enemy_t *e, int16_t px, int16_t py)
{
    // Determine chase vs patrol
    int16_t dx = (int16_t)(px - e->x);
    int16_t dy = (int16_t)(py - e->y);
    int16_t adx = dx < 0 ? (int16_t)-dx : dx;
    int16_t ady = dy < 0 ? (int16_t)-dy : dy;
    bool chase = (adx < 160 && ady < 48);

    if (e->vx == 0) e->vx = 1;  // safe default direction
    int8_t speed = chase ? 2 : 1;

    // In chase mode, orient towards player
    if (chase) {
        if (dx > 0 && e->vx < 0) e->vx = 1;
        else if (dx < 0 && e->vx > 0) e->vx = -1;
    }

    // Tile-based obstacle checks (only when ring buffer covers this area)
    uint16_t ex_pix   = (uint16_t)(e->x);
    uint16_t ey_pix   = (uint16_t)(e->y);
    uint16_t feet_col = (uint16_t)(ex_pix / 8u);
    uint16_t feet_row = (uint16_t)((ey_pix + 16u) / 8u);  // row just below feet

    // Lookahead column in direction of movement
    uint16_t ahead_x  = (e->vx > 0)
        ? (uint16_t)(ex_pix + 16u)   // right edge
        : (uint16_t)(ex_pix - 1u);   // left edge (wraps harmlessly if 0)
    uint16_t ahead_col = ahead_x / 8u;

    // Reverse on wall or platform edge
    bool wall  = solid_at(ahead_col, (uint16_t)(ey_pix / 8u)) ||
                 solid_at(ahead_col, (uint16_t)((ey_pix + 14u) / 8u));
    bool edge  = !solid_at(ahead_col, feet_row);  // no floor ahead
    if (wall || edge) {
        e->vx = -e->vx;
    }

    e->x += (int16_t)(e->vx * speed);

    // World horizontal clamp
    if (e->x < 16) { e->x = 16; e->vx = 1; }
    if (e->x > (int16_t)(WORLD_W_PX - 16)) { e->x = (int16_t)(WORLD_W_PX - 16); e->vx = -1; }

    // Animation: toggle frame every 8 ticks in patrol, 4 in chase
    uint8_t anim_rate = chase ? 4u : 8u;
    if (++e->anim_tick >= anim_rate) {
        e->anim_tick = 0;
        e->anim_frame ^= 1u;
    }
}

static void update_flyer(enemy_t *e, int16_t px, int16_t py)
{
    int16_t dx = (int16_t)(px - e->x);
    int16_t dy = (int16_t)(py - e->y);
    int16_t adx = dx < 0 ? (int16_t)-dx : dx;
    int16_t ady = dy < 0 ? (int16_t)-dy : dy;
    bool chase = (adx < 200 && ady < 150);

    // Horizontal drift toward player in chase, idle patrol stays put
    if (chase) {
        if (dx > 2)       e->x += 1;
        else if (dx < -2) e->x -= 1;
    }

    // Sinusoidal Y bob around spawn_y (slower in patrol, matches player in chase)
    e->y = (int16_t)(e->spawn_y + (int16_t)(bob_y(e->sin_idx) * 3));
    if (chase && ady > 20) {
        if (dy > 0) e->y += 1;
        else        e->y -= 1;
    }

    e->sin_idx = (uint8_t)((e->sin_idx + 1u) & 31u);

    // Animate: alternate bob frames
    if (++e->anim_tick >= 10u) {
        e->anim_tick = 0;
        e->anim_frame ^= 1u;
    }

    // World clamp
    if (e->x < 16) e->x = 16;
    if (e->x > (int16_t)(WORLD_W_PX - 16)) e->x = (int16_t)(WORLD_W_PX - 16);
}

static void update_turret(enemy_t *e, int16_t px, int16_t py)
{
    // Stationary — only switch between armed/idle animation
    int16_t dx = (int16_t)(px - e->x);
    int16_t dy = (int16_t)(py - e->y);
    int16_t adx = dx < 0 ? (int16_t)-dx : dx;
    int16_t ady = dy < 0 ? (int16_t)-dy : dy;
    bool near = (adx < 120 && ady < 80);

    uint8_t rate = near ? 4u : 15u;
    if (++e->anim_tick >= rate) {
        e->anim_tick = 0;
        e->anim_frame ^= 1u;  // frame 4 ↔ 5 (idle ↔ armed)
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void enemy_init(void)
{
    int fd = open("SPAWNS.BIN", O_RDONLY);
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
        e->state     = PATROL;
        e->vx        = (e->type == CRAWLER) ? ((i & 1u) ? (int8_t)1 : (int8_t)-1) : (int8_t)0;
        e->sin_idx   = (uint8_t)(i * 5u);  // stagger flyer phases
        e->anim_frame = 0;
        e->anim_tick  = (uint8_t)(i * 7u); // stagger animation timings
        e->sprite_idx = (uint8_t)(i + 1u); // slots 1-7; slot 0 = player
        e->state_timer = 0;
    }
    close(fd);
    s_num_enemies = count;

    printf("enemy: %u enemies loaded\n", (unsigned)s_num_enemies);
}

void enemy_update_all(int16_t player_x, int16_t player_y)
{
    uint8_t i;
    for (i = 0; i < s_num_enemies; i++) {
        enemy_t *e = &s_enemies[i];

        if (e->state == DEAD) {
            if (e->state_timer > 0u) {
                e->state_timer--;
            } else {
                // Respawn at original position
                e->x         = e->spawn_x;
                e->y         = e->spawn_y;
                e->state     = PATROL;
                e->vx        = (e->type == CRAWLER) ? (int8_t)1 : (int8_t)0;
                e->anim_frame = 0u;
                e->anim_tick  = 0u;
            }
            continue;
        }

        // Update AI by type
        switch (e->type) {
            case CRAWLER: update_crawler(e, player_x, player_y); break;
            case FLYER:   update_flyer  (e, player_x, player_y); break;
            case TURRET:  update_turret (e, player_x, player_y); break;
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
    uint16_t r2 = (uint16_t)radius * (uint16_t)radius;
    uint8_t i;
    for (i = 0; i < s_num_enemies; i++) {
        enemy_t *e = &s_enemies[i];
        if (e->state == DEAD) continue;

        // Quick bounding-box reject before the squared distance check
        int16_t dx = (int16_t)((e->x + 8) - cx);
        int16_t dy = (int16_t)((e->y + 8) - cy);
        if (dx > (int16_t)radius || dx < -(int16_t)radius) continue;
        if (dy > (int16_t)radius || dy < -(int16_t)radius) continue;

        uint16_t d2 = (uint16_t)(dx * dx) + (uint16_t)(dy * dy);
        if (d2 <= r2) {
            e->state      = DEAD;
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
