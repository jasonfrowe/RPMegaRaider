#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "runningman.h"
#include "maze.h"

// ---------------------------------------------------------------------------
// Platformer maze layout
//
//  8 horizontal floors at fixed row spacing.
//  Open air between floors — world defaults to tile 0.
//  Ladders connect adjacent floor pairs (shaft: floor[f+1]..floor[f]-1).
//  Gaps in floors 1-7 let the player drop to the floor below.
//  Solid fill from floor 0 downward acts as the ground.
// ---------------------------------------------------------------------------

#define NUM_FLOORS       8
#define FLOOR_SPACING   17      // rows between floors (136 px > jump height)
#define GROUND_ROW     142      // floor 0 row; rows 142..148 filled solid
#define LADDERS_PER_PAIR 2      // shafts connecting each pair of adjacent floors
#define GAPS_PER_FLOOR   3      // drop-through holes per floor (floors 1–7 only)
#define BORDER_COLS      2      // solid columns on each world edge

// Tile indices
#define TILE_SOLID         1
#define TILE_FLOOR_BOT_MIN 2    // floor tiles, lower world (floors 0–3)
#define TILE_FLOOR_BOT_MAX 7
#define TILE_FLOOR_TOP_MIN 8    // floor/ceiling tiles, upper world (floors 4–7)
#define TILE_FLOOR_TOP_MAX 10
#define TILE_LADDER_BOT  107
#define TILE_LADDER_MID1 108
#define TILE_LADDER_MID2 109
#define TILE_LADDER_TOP  110

// ---------------------------------------------------------------------------
// State (all computed once in maze_generate, then used by compute_tile)
// ---------------------------------------------------------------------------

// floor_row[f] = world row of floor f (f=0 is ground, f=7 is top)
static uint8_t floor_row[NUM_FLOORS];

// gap_col[f][g], gap_w[f][g] — gaps in floor f (only used for f 1..7)
static uint8_t gap_col[NUM_FLOORS][GAPS_PER_FLOOR];
static uint8_t gap_w  [NUM_FLOORS][GAPS_PER_FLOOR];

// ladder_col[f][l] — shaft columns connecting floor f to floor f+1
static uint8_t ladder_col[NUM_FLOORS - 1][LADDERS_PER_PAIR];

// ---------------------------------------------------------------------------
// PRNG
// ---------------------------------------------------------------------------

static uint16_t rng_state;

static uint8_t rng_next(void)
{
    if (!rng_state) rng_state = (uint16_t)lrand();
    rng_state ^= rng_state << 7;
    rng_state ^= rng_state >> 9;
    rng_state ^= rng_state << 8;
    return (uint8_t)rng_state;
}

static uint8_t rng_mod(uint8_t n)
{
    return n <= 1u ? 0u : rng_next() % n;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool col_in_gap(uint8_t col, uint8_t f)
{
    for (uint8_t g = 0; g < GAPS_PER_FLOOR; g++) {
        if (!gap_w[f][g]) continue;
        if (col >= gap_col[f][g] && (uint16_t)col < (uint16_t)gap_col[f][g] + gap_w[f][g])
            return true;
    }
    return false;
}

// If (col, row) is inside a ladder shaft, sets *out_f to the lower floor index
// and returns true.
static bool find_shaft(uint8_t col, uint8_t row, uint8_t *out_f)
{
    for (uint8_t f = 0; f < NUM_FLOORS - 1u; f++) {
        uint8_t top = floor_row[f + 1u]; // top rung = upper floor row
        uint8_t bot = floor_row[f] - 1u; // bottom rung = one row above lower floor
        if (row < top || row > bot) continue;
        for (uint8_t l = 0; l < LADDERS_PER_PAIR; l++) {
            if (ladder_col[f][l] == col) { *out_f = f; return true; }
        }
    }
    return false;
}

static uint8_t floor_tile_id(uint8_t f, uint8_t col)
{
    if (f < 4u)
        return (uint8_t)(2u + (col % 6u));   // tiles 2–7 (lower world)
    else
        return (uint8_t)(8u + (col % 3u));   // tiles 8–10 (upper world)
}

// ---------------------------------------------------------------------------
// Tile classifier — the single source of truth for the tilemap
// ---------------------------------------------------------------------------

static uint8_t compute_tile(uint8_t col, uint8_t row)
{
    // Hard border walls and border rows
    if (col < BORDER_COLS || col >= MAIN_MAP_WIDTH_TILES - BORDER_COLS)
        return TILE_SOLID;
    if (row == 0u || row >= MAIN_MAP_HEIGHT_TILES - 1u)
        return TILE_SOLID;

    // Ceiling line at row 1
    if (row == 1u)
        return (uint8_t)(8u + (col % 3u));

    // Ladder shafts — checked before floor lines so top rung replaces floor tile
    uint8_t sf;
    if (find_shaft(col, row, &sf)) {
        uint8_t top = floor_row[sf + 1u];
        uint8_t bot = floor_row[sf] - 1u;
        if (row == top) return TILE_LADDER_TOP;
        if (row == bot) return TILE_LADDER_BOT;
        return (row & 1u) ? TILE_LADDER_MID1 : TILE_LADDER_MID2;
    }

    // Ground fill: floor 0 and everything below it is solid
    if (row >= floor_row[0])
        return floor_tile_id(0u, col);

    // Floor lines for floors 1..NUM_FLOORS-1
    for (uint8_t f = 1u; f < NUM_FLOORS; f++) {
        if (row == floor_row[f]) {
            if (col_in_gap(col, f)) return 0u; // open hole
            return floor_tile_id(f, col);
        }
    }

    return 0u; // open air
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void maze_generate(void)
{
    rng_state = 0;

    // ------------------------------------------------------------------
    // 1. Floor rows  (floor 0 = ground at GROUND_ROW, each floor up by
    //    FLOOR_SPACING rows)
    // ------------------------------------------------------------------
    for (uint8_t f = 0; f < NUM_FLOORS; f++)
        floor_row[f] = (uint8_t)(GROUND_ROW - (uint8_t)(f * FLOOR_SPACING));

    // ------------------------------------------------------------------
    // 2. Ladder columns
    //    Split horizontal space into (LADDERS_PER_PAIR+1) sections and
    //    place one ladder per section with a random offset.
    // ------------------------------------------------------------------
    for (uint8_t f = 0; f < NUM_FLOORS - 1u; f++) {
        uint8_t usable = MAIN_MAP_WIDTH_TILES - 2u * BORDER_COLS; // 196
        uint8_t sect   = usable / (LADDERS_PER_PAIR + 1u);        // 65
        for (uint8_t l = 0; l < LADDERS_PER_PAIR; l++) {
            uint8_t base = (uint8_t)(BORDER_COLS + sect * (l + 1u));
            ladder_col[f][l] = (uint8_t)(base + rng_mod(sect - 2u));
        }
    }

    // ------------------------------------------------------------------
    // 3. Floor gaps (floors 1–7 only)
    //    Walk across the floor placing gaps at random spacing, skipping
    //    any column used by a ladder on the floors above or below.
    // ------------------------------------------------------------------
    for (uint8_t f = 1u; f < NUM_FLOORS; f++) {
        uint8_t pos = BORDER_COLS + 5u;
        for (uint8_t g = 0; g < GAPS_PER_FLOOR; g++) {
            pos = (uint8_t)(pos + 5u + rng_mod(28u));
            uint8_t w = 4u + rng_mod(5u);

            // Bounds check
            if ((uint16_t)pos + w >= MAIN_MAP_WIDTH_TILES - BORDER_COLS) {
                gap_col[f][g] = 0; gap_w[f][g] = 0; continue;
            }

            // Don't overlap a ladder column from the pair below (f-1) or above (f)
            bool bad = false;
            for (uint8_t ci = 0; ci < 2u && !bad; ci++) {
                uint8_t fi = (ci == 0u) ? (uint8_t)(f - 1u) : f;
                if (fi >= NUM_FLOORS - 1u) continue;
                for (uint8_t l = 0; l < LADDERS_PER_PAIR && !bad; l++) {
                    uint8_t lc = ladder_col[fi][l];
                    if (lc >= pos && (uint16_t)lc < (uint16_t)pos + w) bad = true;
                }
            }

            if (bad) { pos = (uint8_t)(pos + w); gap_col[f][g] = 0; gap_w[f][g] = 0; continue; }

            gap_col[f][g] = pos;
            gap_w[f][g]   = w;
            pos = (uint8_t)(pos + w);
        }
    }

    // ------------------------------------------------------------------
    // 4. Player spawn: ground floor, 3 tiles left of the first ladder up
    // ------------------------------------------------------------------
    {
        uint8_t lc = ladder_col[0][0];
        uint8_t sc = (lc > (uint8_t)(BORDER_COLS + 3u))
                     ? (uint8_t)(lc - 3u) : (uint8_t)(lc + 3u);
        player_start_x = (int16_t)((int16_t)sc * TILE_W);
        player_start_y = (int16_t)((floor_row[0] - 2) * TILE_H);
    }

    // ------------------------------------------------------------------
    // 5. Write tilemap to XRAM (sequential, one byte per tile)
    // ------------------------------------------------------------------
    RIA.addr0 = MAIN_MAP_TILEMAP_DATA;
    RIA.step0 = 1;
    for (uint8_t row = 0; row < MAIN_MAP_HEIGHT_TILES; row++)
        for (uint8_t col = 0; col < MAIN_MAP_WIDTH_TILES; col++)
            RIA.rw0 = compute_tile(col, row);
}

