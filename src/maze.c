#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "runningman.h"
#include "maze.h"

// ---------------------------------------------------------------------------
// Maze parameters
// ---------------------------------------------------------------------------

#define NUM_ZONES        8      // number of horizontal bands
#define LADDERS_PER_GAP  2      // ladder shafts connecting adjacent zones
#define BORDER_COLS      2      // solid columns on each edge

// Tile index conventions
#define TILE_SOLID          1   // generic solid (walls / fill between zones)
#define TILE_FLOOR_BOT_MIN  2   // floor tiles used in bottom half (zones 4-7)
#define TILE_FLOOR_BOT_MAX  7
#define TILE_FLOOR_TOP_MIN  8   // floor/ceiling tiles used in top half (zones 0-3)
#define TILE_FLOOR_TOP_MAX  10
#define TILE_LADDER_BOT   107   // bottom rung
#define TILE_LADDER_MID1  108
#define TILE_LADDER_MID2  109
#define TILE_LADDER_TOP   110   // top rung

// ---------------------------------------------------------------------------
// Zone geometry — computed from MAIN_MAP_HEIGHT_TILES rows
// ---------------------------------------------------------------------------

// Each zone occupies rows [zone_top[z] .. zone_bot[z]] (inclusive).
// The open cavern runs from zone_ceil[z] to zone_floor[z].
static uint8_t zone_top[NUM_ZONES];
static uint8_t zone_bot[NUM_ZONES];
static uint8_t zone_ceil[NUM_ZONES];
static uint8_t zone_floor[NUM_ZONES];

// Two ladder columns connecting zone z to zone z+1
static uint8_t ladder_cols[NUM_ZONES - 1][LADDERS_PER_GAP];

// ---------------------------------------------------------------------------
// PRNG — simple 16-bit xorshift wrapping lrand()
// On first call lrand() seeds it; subsequent calls use the shift register.
// ---------------------------------------------------------------------------

static uint16_t rng_state = 0;

static uint8_t rng_next(void)
{
    if (rng_state == 0)
        rng_state = (uint16_t)lrand();

    rng_state ^= rng_state << 7;
    rng_state ^= rng_state >> 9;
    rng_state ^= rng_state << 8;
    return (uint8_t)(rng_state & 0xFF);
}

// Return a value in [0, range)
static uint8_t rng_mod(uint8_t range)
{
    if (range <= 1) return 0;
    return rng_next() % range;
}

// ---------------------------------------------------------------------------
// Find which zone a given row belongs to (returns NUM_ZONES if out of range)
// ---------------------------------------------------------------------------

static uint8_t zone_for_row(uint8_t row)
{
    for (uint8_t z = 0; z < NUM_ZONES; z++)
        if (row >= zone_top[z] && row <= zone_bot[z])
            return z;
    return NUM_ZONES; // between zones (solid)
}

// ---------------------------------------------------------------------------
// Ladder shaft query
// col in [zone_ceil[z]+1 .. zone_floor[z+1]-1] is within the shaft
// ---------------------------------------------------------------------------

static bool is_ladder_tile(uint8_t col, uint8_t row)
{
    for (uint8_t z = 0; z < NUM_ZONES - 1; z++) {
        for (uint8_t l = 0; l < LADDERS_PER_GAP; l++) {
            if (col != ladder_cols[z][l]) continue;
            // Shaft spans from floor of upper zone (z) up to ceiling of lower zone (z+1)
            uint8_t shaft_top = zone_ceil[z + 1];   // first row of lower zone open area
            uint8_t shaft_bot = zone_floor[z];       // last row of upper zone open area
            if (row < shaft_top || row > shaft_bot) continue;
            return true;
        }
    }
    return false;
}

// Return ladder tile ID for a shaft position (top rung, middle, or bottom rung).
static uint8_t ladder_tile_id(uint8_t col, uint8_t row)
{
    for (uint8_t z = 0; z < NUM_ZONES - 1; z++) {
        for (uint8_t l = 0; l < LADDERS_PER_GAP; l++) {
            if (col != ladder_cols[z][l]) continue;
            uint8_t shaft_top = zone_ceil[z + 1];
            uint8_t shaft_bot = zone_floor[z];
            if (row < shaft_top || row > shaft_bot) continue;
            if (row == shaft_top) return TILE_LADDER_TOP;
            if (row == shaft_bot) return TILE_LADDER_BOT;
            // Alternate mid tiles for visual variety
            return ((row & 1u) ? TILE_LADDER_MID1 : TILE_LADDER_MID2);
        }
    }
    return 0; // not a ladder column/row
}

// ---------------------------------------------------------------------------
// Tile classification for a given (col, row)
// ---------------------------------------------------------------------------

static uint8_t compute_tile(uint8_t col, uint8_t row)
{
    // Hard borders — always solid
    if (col < BORDER_COLS || col >= MAIN_MAP_WIDTH_TILES - BORDER_COLS)
        return TILE_SOLID;
    if (row < 1 || row >= MAIN_MAP_HEIGHT_TILES - 1)
        return TILE_SOLID;

    uint8_t z = zone_for_row(row);

    if (z == NUM_ZONES) {
        // Row falls in the buffer space between zones — solid
        return TILE_SOLID;
    }

    uint8_t ceil_row  = zone_ceil[z];
    uint8_t floor_row = zone_floor[z];

    // Ceiling tile
    if (row == ceil_row) {
        // Top half of world (zones 0-3): tiles 8-10; bottom half (zones 4-7): tile 1
        return (z < 4) ? (uint8_t)(TILE_FLOOR_TOP_MIN + (col % 3u))
                       : TILE_SOLID;
    }

    // Floor tile
    if (row == floor_row) {
        // Bottom half (zones 4-7): varied decorative floor tiles 2-7
        // Top half (zones 0-3): tiles 8-10
        if (z >= 4)
            return (uint8_t)(TILE_FLOOR_BOT_MIN + (col % (TILE_FLOOR_BOT_MAX - TILE_FLOOR_BOT_MIN + 1u)));
        else
            return (uint8_t)(TILE_FLOOR_TOP_MIN + (col % 3u));
    }

    // Rows above ceiling or below floor → solid fill between zones
    if (row < ceil_row || row > floor_row)
        return TILE_SOLID;

    // Open cavern space — check for ladder first
    if (is_ladder_tile(col, row))
        return ladder_tile_id(col, row);

    // Sparse pillars / stalactites for visual interest (deterministic hash)
    // Using a simple arithmetic hash to avoid storing extra state.
    // The hash avoids col 2-5 to keep the player spawn area clear.
    if (col > 5 && col < MAIN_MAP_WIDTH_TILES - 5) {
        uint16_t hash = (uint16_t)col * 7u + (uint16_t)z * 31u;
        if ((hash % 20u) < 2u)
            return TILE_SOLID;
    }

    return 0; // open space
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void maze_generate(void)
{
    // Seed the RNG fresh each run
    rng_state = 0;

    // -----------------------------------------------------------------------
    // 1. Compute zone boundaries — divide 150 rows into NUM_ZONES bands.
    //    Reserve 1 row top and 1 row bottom as hard border.
    // -----------------------------------------------------------------------
    {
        uint8_t usable_rows = MAIN_MAP_HEIGHT_TILES - 2; // rows 1..148
        uint8_t base_h      = usable_rows / NUM_ZONES;   // ~18 rows each
        uint8_t remainder   = usable_rows % NUM_ZONES;

        uint8_t cur = 1; // first non-border row
        for (uint8_t z = 0; z < NUM_ZONES; z++) {
            uint8_t h = base_h + (z < remainder ? 1u : 0u);
            zone_top[z] = cur;
            zone_bot[z] = cur + h - 1u;
            cur = zone_bot[z] + 1u;

            // Open cavern: ceiling 1-2 rows below zone top, floor 1-2 rows above zone bottom
            uint8_t ceil_off  = 1u + rng_mod(2u);
            uint8_t floor_off = 1u + rng_mod(2u);
            zone_ceil[z]  = zone_top[z]  + ceil_off;
            zone_floor[z] = zone_bot[z]  - floor_off;

            // Guard: cavern must be at least 4 tiles tall
            if (zone_floor[z] <= zone_ceil[z] + 4u) {
                zone_ceil[z]  = zone_top[z]  + 1u;
                zone_floor[z] = zone_bot[z]  - 1u;
            }
        }
    }

    // -----------------------------------------------------------------------
    // 2. Place ladder shafts between adjacent zones.
    //    Pick LADDERS_PER_GAP distinct columns per gap, well inside the border.
    // -----------------------------------------------------------------------
    for (uint8_t z = 0; z < NUM_ZONES - 1; z++) {
        for (uint8_t l = 0; l < LADDERS_PER_GAP; l++) {
            uint8_t col;
            uint8_t tries = 0;
            do {
                // Random column in a third-width band to space them out
                uint8_t band_w = (MAIN_MAP_WIDTH_TILES - 2u * BORDER_COLS) / (LADDERS_PER_GAP + 1u);
                col = (uint8_t)(BORDER_COLS + band_w * (l + 1u) + rng_mod(band_w / 2u));
                tries++;
                // Ensure no duplicate in this gap
                bool dup = false;
                for (uint8_t k = 0; k < l; k++)
                    if (ladder_cols[z][k] == col) { dup = true; break; }
                if (!dup) break;
            } while (tries < 16);
            ladder_cols[z][l] = col;
        }
    }

    // -----------------------------------------------------------------------
    // 3. Set player spawn — zone 7 (bottom), column 4, 2 tiles above floor.
    // -----------------------------------------------------------------------
    player_start_x = (int16_t)(4 * TILE_W);
    player_start_y = (int16_t)((zone_floor[NUM_ZONES - 1] - 2) * TILE_H);

    // -----------------------------------------------------------------------
    // 4. Write tilemap to XRAM sequentially (fastest path).
    // -----------------------------------------------------------------------
    RIA.addr0 = MAIN_MAP_TILEMAP_DATA;
    RIA.step0 = 1;
    for (uint8_t row = 0; row < MAIN_MAP_HEIGHT_TILES; row++) {
        for (uint8_t col = 0; col < MAIN_MAP_WIDTH_TILES; col++) {
            RIA.rw0 = compute_tile(col, row);
        }
    }
}
