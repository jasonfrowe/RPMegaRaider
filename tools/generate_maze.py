#!/usr/bin/env python3
"""
generate_maze.py — Generate 800×600-tile platformer world for RPMegaRaider.

Produces column-major binary files (800 cols × 600 rows):
  MAZE_FG.BIN   480,000 bytes — collision/FG tile layer
  MAZE_BG.BIN   480,000 bytes — decorative BG tile layer

Also produces:
  SPAWNS.BIN    1 + N×5 bytes — enemy spawn positions
                  1 byte count, then N records of (uint16_t x_px, uint16_t y_px, uint8 type)
                  type: 0=Crawler  1=Flyer  2=Turret

Column-major layout: byte at offset (col * WORLD_H + row) is tile at (col, row).
This is optimal for the horizontal-scroll streaming engine (1 seek + 1 read per column).

FG tile meanings (match constants.h and generate_fg_tiles.py):
  0         = empty air (transparent, shows BG through)
  1-30      = solid wall / platform (blocks player); zone-specific appearance
  31        = charge pack pickup (collect for EMP charge)
  32        = memory shard pickup (collect 5 to unlock terminus)
  33        = terminus (reach with all 5 shards to WIN)
  107-110   = ladder tiles (climbable shafts)

FG zone layout (matched to generate_fg_tiles.py):
  1-10  DEEP  (world row >= 400): volcanic basalt
  11-20 MID   (world row >= 200): dungeon brick
  21-30 UPPER (world row <  200): ancient carved stone + crystal

BG tile meanings (generate_bg_tiles.py — circuit board theme):
  0   BG_SUBSTRATE      — dark PCB background
  1   BG_SUBSTRATE_ALT  — substrate with faint texture
  2   BG_TRACE_H        — horizontal gold trace
  3   BG_TRACE_V        — vertical gold trace
  4   BG_CORNER_A       — trace corner right+up
  5   BG_CORNER_B       — trace corner left+up
  6   BG_VIA            — solder via
  7   BG_T_JUNC         — T-junction
  8   BG_IC_BODY        — IC chip body
  9   BG_IC_PINS        — IC chip with pins
  10  BG_PAD            — copper component pad
  11  BG_CAPACITOR      — capacitor
  12  BG_LED_RED        — red LED
  13  BG_LED_GREEN      — green LED
  14  BG_TRACE_VIA      — H trace + via blob
  15  BG_CROSS          — cross junction
"""

import random, struct, os
from collections import deque

# ---------------------------------------------------------------------------
# World constants
# ---------------------------------------------------------------------------
WORLD_W     = 800   # columns (tiles)
WORLD_H     = 600   # rows (tiles)

NUM_FLOORS          = 30
MIN_FLOOR_SPACING   = 8     # minimum rows between floors
MAX_FLOOR_SPACING   = 14    # maximum rows between floors
GROUND_ROW          = 560   # floor[0] — where the player starts (row 560)
BORDER_COLS     = 2     # solid columns on each world edge

LADDERS_PER_PAIR = 8    # ladder shafts connecting adjacent floor pairs
GAPS_PER_FLOOR   = 3    # drop-through gaps per floor (not ground floor)
MIN_GAP_SPACING  = 15   # minimum tiles between gap starts
GAP_WIDTH_MIN    = 4
GAP_WIDTH_MAX    = 8

# FG tile IDs
TILE_EMPTY       = 0
TILE_SOLID       = 1    # dark basalt — borders and generic solid
TILE_LADDER_TOP  = 107
TILE_LADDER_MID1 = 108
TILE_LADDER_MID2 = 109
TILE_LADDER_BOT  = 110

# Pickup / special tile IDs
TILE_CHARGE_PACK  = 31
TILE_MEMORY_SHARD = 32
TILE_TERMINUS     = 33
TILE_PORTAL_MIN   = 34   # portal archway 4×4 grid (tiles 34-49)
TILE_PORTAL_MAX   = 49

# Player start tile coords (must match player_start_x/y in runningman.c)
PLAYER_START_COL = 10   # 80px / 8 = 10
PLAYER_START_ROW = 557  # one tile above 4464px/8=558 so it's above player's head

# Enemy spawn types
SPAWN_CRAWLER = 0
SPAWN_FLYER   = 1
SPAWN_TURRET  = 2

# FG zone boundaries (world row)
ZONE_DEEP_MIN  = 400   # rows >= 400: volcanic deep
ZONE_MID_MIN   = 200   # rows >= 200: dungeon mid
                        # rows <  200: ancient upper

# BG tile IDs — circuit board theme (match generate_bg_tiles.py)
BG_SUBSTRATE     = 0
BG_SUBSTRATE_ALT = 1
BG_TRACE_H       = 2
BG_TRACE_V       = 3
BG_CORNER_A      = 4
BG_CORNER_B      = 5
BG_VIA           = 6
BG_T_JUNC        = 7
BG_IC_BODY       = 8
BG_IC_PINS       = 9
BG_PAD           = 10
BG_CAPACITOR     = 11
BG_LED_RED       = 12
BG_LED_GREEN     = 13
BG_TRACE_VIA     = 14
BG_CROSS         = 15

# Circuit board trace grid spacing
BG_H_SPACING = 10   # H trace every 10 rows
BG_V_SPACING = 8    # V trace every 8 cols
BG_H_OFFSET  = 4    # first H trace row
BG_V_OFFSET  = 3    # first V trace col

# ---------------------------------------------------------------------------
# RNG (same xorshift16 as maze.c for reproducibility)
# ---------------------------------------------------------------------------
_rng = [0]

def seed_rng(s):
    _rng[0] = s & 0xFFFF or 1

def rng_next():
    s = _rng[0]
    s ^= (s << 7) & 0xFFFF
    s ^= (s >> 9) & 0xFFFF
    s ^= (s << 8) & 0xFFFF
    _rng[0] = s & 0xFFFF
    return _rng[0] & 0xFF

def rng_mod(n):
    return 0 if n <= 1 else rng_next() % n

# ---------------------------------------------------------------------------
# World arrays
# ---------------------------------------------------------------------------
fg = bytearray(WORLD_W * WORLD_H)   # fg[col * WORLD_H + row]
bg = bytearray(WORLD_W * WORLD_H)

def fg_set(col, row, tile):
    if 0 <= col < WORLD_W and 0 <= row < WORLD_H:
        fg[col * WORLD_H + row] = tile

def bg_set(col, row, tile):
    if 0 <= col < WORLD_W and 0 <= row < WORLD_H:
        bg[col * WORLD_H + row] = tile

def fg_get(col, row):
    if 0 <= col < WORLD_W and 0 <= row < WORLD_H:
        return fg[col * WORLD_H + row]
    return TILE_SOLID

def bg_get(col, row):
    if 0 <= col < WORLD_W and 0 <= row < WORLD_H:
        return bg[col * WORLD_H + row]
    return BG_SUBSTRATE

def zone_platform_tile(row):
    """Return zone-appropriate platform-top FG tile for a given world row."""
    if row >= ZONE_DEEP_MIN:   return 3   # deep_platform_top
    elif row >= ZONE_MID_MIN:  return 13  # mid_platform_top
    else:                      return 23  # upper_platform_top

def zone_fill_tile(row):
    """Return zone-appropriate fill FG tile for a given world row."""
    if row >= ZONE_DEEP_MIN:   return 4   # deep_fill_a
    elif row >= ZONE_MID_MIN:  return 14  # mid_fill_a
    else:                      return 24  # upper_fill_a

def fill_fg_rect(col0, row0, col1, row1, tile):
    for c in range(col0, col1 + 1):
        for r in range(row0, row1 + 1):
            fg_set(c, r, tile)

def fill_bg_rect(col0, row0, col1, row1, tile):
    for c in range(col0, col1 + 1):
        for r in range(row0, row1 + 1):
            bg_set(c, r, tile)

def draw_ladder(col, r_top, r_bot):
    """Draw a ladder shaft from the upper floor row down to one row above lower floor."""
    if col <= BORDER_COLS or col >= WORLD_W - BORDER_COLS:
        return

    shaft_top = r_top
    shaft_bot = r_bot - 1
    if shaft_bot <= shaft_top:
        return

    fg_set(col, shaft_top, TILE_LADDER_TOP)
    for row in range(shaft_top + 1, shaft_bot):
        fg_set(col, row, TILE_LADDER_MID1 if (row & 1) else TILE_LADDER_MID2)
    fg_set(col, shaft_bot, TILE_LADDER_BOT)

# ---------------------------------------------------------------------------
# Layout generator
# ---------------------------------------------------------------------------
def generate():
    seed_rng(0xBEEF)

    # ------------------------------------------------------------------
    # 1. Background fill — circuit board PCB pattern.
    # ------------------------------------------------------------------
    # Lay down the trace grid: H traces every BG_H_SPACING rows,
    # V traces every BG_V_SPACING cols. Intersections become BG_CROSS.
    for col in range(WORLD_W):
        for row in range(WORLD_H):
            dr = (row - BG_H_OFFSET) % BG_H_SPACING
            dc = (col - BG_V_OFFSET) % BG_V_SPACING
            on_h = dr == 0 or dr == 1   # 2-pixel-wide H trace
            on_v = dc == 0 or dc == 1   # 2-pixel-wide V trace
            if on_h and on_v:
                t = BG_CROSS
            elif on_h:
                t = BG_TRACE_H
            elif on_v:
                t = BG_TRACE_V
            else:
                # Alternate between substrate variants for faint texture
                t = BG_SUBSTRATE_ALT if (col * 3 + row * 5) % 17 == 0 else BG_SUBSTRATE
            bg_set(col, row, t)

    # Place vias at every other H/V intersection
    for row in range(BG_H_OFFSET, WORLD_H, BG_H_SPACING * 2):
        for col in range(BG_V_OFFSET, WORLD_W, BG_V_SPACING * 3):
            bg_set(col, row, BG_VIA)

    # ------------------------------------------------------------------
    # 2. Floor rows: evenly spaced with random offset, bottom up.
    #    floor[0] = GROUND_ROW (ground level)
    #    floor[f] for f > 0 is above floor[f-1]
    # ------------------------------------------------------------------
    floor_row = [0] * NUM_FLOORS
    floor_row[0] = GROUND_ROW

    for f in range(1, NUM_FLOORS):
        spacing = MIN_FLOOR_SPACING + rng_mod(MAX_FLOOR_SPACING - MIN_FLOOR_SPACING + 1)
        floor_row[f] = floor_row[f - 1] - spacing
        if floor_row[f] < 4:
            floor_row[f] = 4

    # ------------------------------------------------------------------
    # 3. Solid border walls and top/bottom border rows.
    # ------------------------------------------------------------------
    for col in range(WORLD_W):
        fg_set(col, 0, TILE_SOLID)
        fg_set(col, WORLD_H - 1, TILE_SOLID)

    for row in range(WORLD_H):
        for c in range(BORDER_COLS):
            fg_set(c, row, TILE_SOLID)
            fg_set(WORLD_W - 1 - c, row, TILE_SOLID)

    # ------------------------------------------------------------------
    # 4. Ground fill: solid from floor[0] downward.
    # ------------------------------------------------------------------
    ground_fill_tile = zone_fill_tile(floor_row[0])  # deep fill (row 560)
    for col in range(BORDER_COLS, WORLD_W - BORDER_COLS):
        for row in range(floor_row[0], WORLD_H - 1):
            fg_set(col, row, ground_fill_tile)

    # ------------------------------------------------------------------
    # 5. Floor lines for floors 1..NUM_FLOORS-1.
    # ------------------------------------------------------------------
    # For each floor f, store gap locations so ladders can avoid them.
    gap_cols = [[] for _ in range(NUM_FLOORS)]   # list of (start_col, end_col) pairs

    for f in range(1, NUM_FLOORS):
        r = floor_row[f]
        if r < 2 or r >= WORLD_H - 1:
            continue  # floor out of world bounds

        platform_tile = zone_platform_tile(r)  # zone-specific top tile
        bg_row_above = r - 1  # used for decorations below

        # Place gaps first (randomly spaced)
        pos = BORDER_COLS + rng_mod(MIN_GAP_SPACING)
        gaps_placed = []
        for _ in range(GAPS_PER_FLOOR):
            if pos >= WORLD_W - BORDER_COLS - GAP_WIDTH_MIN:
                break
            w = GAP_WIDTH_MIN + rng_mod(GAP_WIDTH_MAX - GAP_WIDTH_MIN + 1)
            end = min(pos + w, WORLD_W - BORDER_COLS)
            gaps_placed.append((pos, end))
            pos = end + MIN_GAP_SPACING + rng_mod(MIN_GAP_SPACING)

        gap_cols[f] = gaps_placed

        # Draw the floor line (skipping gap regions)
        for col in range(BORDER_COLS, WORLD_W - BORDER_COLS):
            in_gap = any(gs <= col < ge for gs, ge in gaps_placed)
            if not in_gap:
                fg_set(col, r, platform_tile)

        # Occasionally add a component pad below floor lines
        for col in range(BORDER_COLS + 4, WORLD_W - BORDER_COLS - 4, 13 + rng_mod(9)):
            in_gap = any(gs <= col < ge for gs, ge in gaps_placed)
            if not in_gap and fg_get(col, r + 1) == TILE_EMPTY:
                if bg_get(col, r + 1) in (BG_SUBSTRATE, BG_SUBSTRATE_ALT):
                    bg_set(col, r + 1, BG_PAD)

    # ------------------------------------------------------------------
    # 6. Ladder shafts connecting adjacent floor pairs.
    # ------------------------------------------------------------------
    ladder_col_set = [[] for _ in range(NUM_FLOORS - 1)]
    forced_start_ladder_col = None

    for f in range(NUM_FLOORS - 1):
        r_bot = floor_row[f]
        r_top = floor_row[f + 1]
        if r_top < 2 or r_bot >= WORLD_H - 1 or r_bot <= r_top:
            continue

        # Ensure the first screen has an obvious climb route right of spawn.
        if f == 0:
            forced_candidates = [
                PLAYER_START_COL + 9,
                PLAYER_START_COL + 12,
                PLAYER_START_COL + 6,
            ]
            for forced_col in forced_candidates:
                if forced_col >= WORLD_W - BORDER_COLS:
                    continue
                gaps_top = gap_cols[f + 1] if f + 1 < NUM_FLOORS else []
                if any(gs <= forced_col < ge for gs, ge in gaps_top):
                    continue
                ladder_col_set[f].append(forced_col)
                draw_ladder(forced_col, r_top, r_bot)
                forced_start_ladder_col = forced_col
                break

        usable = WORLD_W - 2 * BORDER_COLS
        sect   = usable // (LADDERS_PER_PAIR + 1)

        for l in range(LADDERS_PER_PAIR):
            base = BORDER_COLS + sect * (l + 1)
            col  = base + rng_mod(max(1, sect - 4))

            # Ensure ladder column is not in a gap of either adjacent floor
            gaps_bot = gap_cols[f]   if f < NUM_FLOORS else []
            gaps_top = gap_cols[f+1] if f + 1 < NUM_FLOORS else []
            in_gap = (any(gs <= col < ge for gs, ge in gaps_bot) or
                      any(gs <= col < ge for gs, ge in gaps_top))
            if in_gap:
                col = base  # fallback to section base

            ladder_col_set[f].append(col)
            draw_ladder(col, r_top, r_bot)

    # ------------------------------------------------------------------
    # 7. Vertical walls — dead ends spanning floor-to-floor
    # ------------------------------------------------------------------
    # Place walls deterministically across left/middle/right map sections
    # to ensure balanced dead-end distribution (not just left side).
    walls_placed = []
    for f in range(1, NUM_FLOORS - 1):
        r_this = floor_row[f]
        r_above = floor_row[f + 1]
        if r_this < 6 or r_above < 2 or r_above >= r_this:
            continue
        # Gather ladder columns near this floor so we don't block them
        nearby_ladders = set()
        if f - 1 < len(ladder_col_set):
            for lc in ladder_col_set[f - 1]:
                for dx in range(-3, 4):
                    nearby_ladders.add(lc + dx)
        if f < len(ladder_col_set):
            for lc in ladder_col_set[f]:
                for dx in range(-3, 4):
                    nearby_ladders.add(lc + dx)

        # Divide map into 5 sections and place 2-3 walls per floor across sections,
        # cycling through sections to spread them across the map width.
        usable = WORLD_W - 2 * BORDER_COLS
        sect = usable // 5
        
        num_walls = 2 + rng_mod(2)  # 2-3 walls per floor
        walls_this_floor = 0
        
        for s_idx in range(5):
            if walls_this_floor >= num_walls:
                break
            # Pick a column in this section with random offset
            base = BORDER_COLS + sect * s_idx + sect // 2
            col = base + rng_mod(sect // 2 + 1) - (sect // 4)
            col = max(BORDER_COLS + 4, min(WORLD_W - BORDER_COLS - 4, col))
            
            if fg_get(col, r_this) == TILE_EMPTY:  # gap — skip
                continue
            if col in nearby_ladders:
                continue
            # Wall spans from one tile above this floor up to the floor above
            wall_tiles = []
            for row in range(r_above, r_this):
                if row < 1:
                    continue
                if fg_get(col, row) == TILE_EMPTY:
                    fg_set(col, row, zone_fill_tile(row))
                    wall_tiles.append((col, row))
            if wall_tiles:
                walls_placed.append(wall_tiles)
                walls_this_floor += 1

    # ------------------------------------------------------------------
    # 8. Reachability check — remove walls that isolate floor sections
    # ------------------------------------------------------------------
    def flood_fill(sc, sr):
        vis = bytearray(WORLD_W * WORLD_H)
        q = deque([(sc, sr)])
        vis[sc * WORLD_H + sr] = 1
        while q:
            c, r = q.popleft()
            for dc, dr in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                nc, nr = c + dc, r + dr
                if 0 <= nc < WORLD_W and 0 <= nr < WORLD_H:
                    idx = nc * WORLD_H + nr
                    if not vis[idx] and not (1 <= fg[idx] <= 30):
                        vis[idx] = 1
                        q.append((nc, nr))
        return vis

    start_air_row = floor_row[0] - 1
    reachable = flood_fill(PLAYER_START_COL, start_air_row)

    # Remove walls (newest first) until every floor is reachable
    while walls_placed:
        all_ok = True
        for f in range(1, NUM_FLOORS):
            r = floor_row[f]
            if r < 2:
                continue
            check_row = r - 1
            found = False
            for c in range(BORDER_COLS + 2, WORLD_W - BORDER_COLS - 2, 5):
                if reachable[c * WORLD_H + check_row]:
                    found = True
                    break
            if not found:
                all_ok = False
                break
        if all_ok:
            break
        last = walls_placed.pop()
        for (c, r) in last:
            fg_set(c, r, TILE_EMPTY)
        reachable = flood_fill(PLAYER_START_COL, start_air_row)

    # ------------------------------------------------------------------
    # 9. BG circuit board components — ICs, LEDs, capacitors.
    # ------------------------------------------------------------------
    seed_rng(0xCAFE)   # fresh seed for component placement

    # IC blocks: 2×1 tiles (body + pins)
    for _ in range(300):
        col = BORDER_COLS + 1 + rng_mod(WORLD_W - 2 * BORDER_COLS - 3)
        row = 2 + rng_mod(WORLD_H - 4)
        if fg_get(col, row) != TILE_EMPTY or fg_get(col + 1, row) != TILE_EMPTY:
            continue
        if (bg_get(col, row) in (BG_SUBSTRATE, BG_SUBSTRATE_ALT) and
                bg_get(col + 1, row) in (BG_SUBSTRATE, BG_SUBSTRATE_ALT)):
            bg_set(col,     row, BG_IC_BODY)
            bg_set(col + 1, row, BG_IC_PINS)

    # LEDs and capacitors at substrate tiles
    for _ in range(600):
        col = BORDER_COLS + rng_mod(WORLD_W - 2 * BORDER_COLS)
        row = 2 + rng_mod(WORLD_H - 4)
        if fg_get(col, row) != TILE_EMPTY:
            continue
        if bg_get(col, row) in (BG_SUBSTRATE, BG_SUBSTRATE_ALT):
            c = rng_mod(3)
            bg_set(col, row, (BG_LED_RED, BG_LED_GREEN, BG_CAPACITOR)[c])

    # T-junctions on H-traces near floors (circuit routing feel)
    for f in range(1, NUM_FLOORS):
        r = floor_row[f]
        if r < 3:
            continue
        for col in range(BORDER_COLS + 6, WORLD_W - BORDER_COLS - 6, 16 + rng_mod(12)):
            if (fg_get(col, r + 2) == TILE_EMPTY and
                    bg_get(col, r + 2) == BG_TRACE_H):
                bg_set(col, r + 2, BG_T_JUNC)

    # ------------------------------------------------------------------
    # 10. Pickup and spawn placement.
    # ------------------------------------------------------------------
    seed_rng(0xDEAD)   # fresh seed for pickup / spawn placement

    # --- 3 guidance sparks right of the start ladder over ~3 screen widths ---
    # These are charge packs on the ground path to pull the player right.
    ground_row = floor_row[0]
    spark_row = ground_row - 1
    spark_anchor = forced_start_ladder_col if forced_start_ladder_col is not None else (PLAYER_START_COL + 9)
    spark_targets = [spark_anchor + 12, spark_anchor + 52, spark_anchor + 92]

    for target in spark_targets:
        placed = False
        for delta in [0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6]:
            col = target + delta
            if col <= BORDER_COLS + 2 or col >= WORLD_W - BORDER_COLS - 2:
                continue
            if fg_get(col, spark_row) != TILE_EMPTY:
                continue
            if fg_get(col, ground_row) == TILE_EMPTY:
                continue
            fg_set(col, spark_row, TILE_CHARGE_PACK)
            placed = True
            break
        if not placed:
            fallback = min(max(target, BORDER_COLS + 3), WORLD_W - BORDER_COLS - 3)
            if fg_get(fallback, ground_row) != TILE_EMPTY and fg_get(fallback, spark_row) == TILE_EMPTY:
                fg_set(fallback, spark_row, TILE_CHARGE_PACK)

    # --- 5 Memory Shards on high floors (floors 22-26) ---
    shard_floors = [22, 23, 24, 25, 26]
    for f in shard_floors:
        if f >= NUM_FLOORS:
            continue
        r = floor_row[f]
        if r < 2:
            continue
        shard_row = r - 1
        for _ in range(30):
            col = BORDER_COLS + 10 + rng_mod(WORLD_W - 2 * BORDER_COLS - 20)
            if (fg_get(col, shard_row) == TILE_EMPTY and
                fg_get(col, r) != TILE_EMPTY and
                reachable[col * WORLD_H + shard_row]):
                fg_set(col, shard_row, TILE_MEMORY_SHARD)
                break

    # --- 1 Terminus near top-center (floor NUM_FLOORS-2) ---
    term_col = WORLD_W // 2
    term_row = 2
    top_f = NUM_FLOORS - 2
    if top_f >= 0 and top_f < NUM_FLOORS:
        term_row = floor_row[top_f] - 1
        for _ in range(50):
            tc = WORLD_W // 2 + rng_mod(40) - 20
            if (fg_get(tc, term_row) == TILE_EMPTY and
                fg_get(tc, floor_row[top_f]) != TILE_EMPTY and
                reachable[tc * WORLD_H + term_row]):
                term_col = tc
                fg_set(term_col, term_row, TILE_TERMINUS)
                break

    # --- Entrance portal archway (4x4 grid) centered on player ---
    portal_base = floor_row[0] - 1    # just above ground surface
    portal_left = PLAYER_START_COL - 1  # cols 9-12, player at col 10
    for pr in range(4):
        for pc in range(4):
            tile_id = TILE_PORTAL_MIN + pr * 4 + pc
            r = portal_base - (3 - pr)
            c = portal_left + pc
            if 0 <= r < WORLD_H and 0 <= c < WORLD_W:
                fg_set(c, r, tile_id)

    # --- Exit portal archway (4x4 grid) on the floor, offset right of terminus ---
    # Place portal on the same floor level as entrance (base row = floor_row[top_f] - 1),
    # but offset right so terminus tile remains visible and playable.
    if top_f >= 0 and top_f < NUM_FLOORS:
        exit_base = floor_row[top_f] - 1  # same level as entrance
        exit_left = term_col + 8  # 8 tiles to the right of terminus
        if exit_left + 3 >= WORLD_W - BORDER_COLS:  # too close to right edge
            exit_left = term_col - 8  # place to the left instead
        
        for pr in range(4):
            for pc in range(4):
                tile_id = TILE_PORTAL_MIN + pr * 4 + pc
                r = exit_base - (3 - pr)
                c = exit_left + pc
                if 0 <= r < WORLD_H and 0 <= c < WORLD_W:
                    # Do NOT skip the terminus — just don't place portal on it
                    if r == term_row and c == term_col:
                        continue
                    fg_set(c, r, tile_id)

    # --- Charge Packs: dense, floor-by-floor spread with upper-floor bias ---
    # Use deterministic sections across each floor so the top of the maze
    # always contains pickups instead of depending on random retries.
    for f in range(1, NUM_FLOORS - 1):
        r = floor_row[f]
        if r < 2:
            continue
        pack_row = r - 1

        if f >= 20:
            target = 8
        elif f >= 10:
            target = 6
        else:
            target = 5

        usable = WORLD_W - 2 * BORDER_COLS
        sect = max(1, usable // (target + 1))
        placed = 0

        for s in range(target):
            base = BORDER_COLS + sect * (s + 1)
            candidate_offsets = [0, -3, 3, -6, 6, -9, 9]
            for off in candidate_offsets:
                col = base + off
                if col <= BORDER_COLS + 1 or col >= WORLD_W - BORDER_COLS - 1:
                    continue
                if fg_get(col, pack_row) != TILE_EMPTY:
                    continue
                if fg_get(col, r) == TILE_EMPTY:
                    continue
                fg_set(col, pack_row, TILE_CHARGE_PACK)
                placed += 1
                break

        # Fallback random fill if a floor still couldn't reach target.
        tries = 0
        while placed < target and tries < 180:
            tries += 1
            col = BORDER_COLS + 5 + rng_mod(WORLD_W - 2 * BORDER_COLS - 10)
            if fg_get(col, pack_row) != TILE_EMPTY:
                continue
            if fg_get(col, r) == TILE_EMPTY:
                continue
            fg_set(col, pack_row, TILE_CHARGE_PACK)
            placed += 1

    # --- Enemy spawns ---
    # Place 3 Crawlers on mid floors (floors 3-14), spaced across world
    # Place 2 Flyers in open mid-air between floors
    # Place 2 Turrets on elevated floors (floors 15-20)
    spawns = []   # list of (x_px, y_px, type)

    crawler_floors = [3, 8, 13]
    crawler_col_offsets = [WORLD_W // 5, WORLD_W * 2 // 5, WORLD_W * 3 // 5]
    for i, f in enumerate(crawler_floors):
        if f >= NUM_FLOORS:
            continue
        r = floor_row[f]
        if r < 2:
            continue
        col = crawler_col_offsets[i] + rng_mod(40) - 20
        col = max(BORDER_COLS + 4, min(WORLD_W - BORDER_COLS - 5, col))
        x_px = col * 8
        y_px = (r - 2) * 8   # stand 2 tiles above platform surface (sprite is 16px = 2 tiles)
        spawns.append((x_px, y_px, SPAWN_CRAWLER))

    flyer_floors = [6, 16]
    flyer_col_offsets = [WORLD_W // 3, WORLD_W * 2 // 3]
    for i, f in enumerate(flyer_floors):
        if f + 1 >= NUM_FLOORS:
            continue
        r_bot = floor_row[f]
        r_top = floor_row[f + 1]
        mid_row = (r_bot + r_top) // 2
        col = flyer_col_offsets[i] + rng_mod(60) - 30
        col = max(BORDER_COLS + 4, min(WORLD_W - BORDER_COLS - 5, col))
        x_px = col * 8
        y_px = mid_row * 8
        spawns.append((x_px, y_px, SPAWN_FLYER))

    turret_floors = [15, 19]
    turret_col_offsets = [WORLD_W * 3 // 4, WORLD_W // 4]
    for i, f in enumerate(turret_floors):
        if f >= NUM_FLOORS:
            continue
        r = floor_row[f]
        if r < 2:
            continue
        col = turret_col_offsets[i] + rng_mod(30) - 15
        col = max(BORDER_COLS + 4, min(WORLD_W - BORDER_COLS - 5, col))
        x_px = col * 8
        y_px = (r - 2) * 8
        spawns.append((x_px, y_px, SPAWN_TURRET))

    # ------------------------------------------------------------------
    # 11. Player start position output.
    # ------------------------------------------------------------------
    # Player starts on floor[0] (ground level, row 560).
    # Sprite top is 2 tiles above the floor surface.
    start_x = PLAYER_START_COL * 8            # pixel x (80)
    start_y = (floor_row[0] - 2) * 8         # pixel y (4464)

    return start_x, start_y, spawns

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
start_x, start_y, spawns = generate()

out_dir = os.path.dirname(__file__) or '.'
out_root = os.path.join(out_dir, '..', 'images')
os.makedirs(out_root, exist_ok=True)

with open(os.path.join(out_root, 'MAZE_FG.BIN'), 'wb') as f:
    f.write(fg)
print(f"MAZE_FG.BIN: {len(fg)} bytes")

with open(os.path.join(out_root, 'MAZE_BG.BIN'), 'wb') as f:
    f.write(bg)
print(f"MAZE_BG.BIN: {len(bg)} bytes")

# Write row-major companions used for fast vertical scroll (1 seek + WORLD_W bytes per row).
fg_rows = bytearray(WORLD_W * WORLD_H)
bg_rows = bytearray(WORLD_W * WORLD_H)
for col in range(WORLD_W):
    for row in range(WORLD_H):
        fg_rows[row * WORLD_W + col] = fg[col * WORLD_H + row]
        bg_rows[row * WORLD_W + col] = bg[col * WORLD_H + row]

with open(os.path.join(out_root, 'MAZE_FG_ROWS.BIN'), 'wb') as f:
    f.write(fg_rows)
print(f"MAZE_FG_ROWS.BIN: {len(fg_rows)} bytes")

with open(os.path.join(out_root, 'MAZE_BG_ROWS.BIN'), 'wb') as f:
    f.write(bg_rows)
print(f"MAZE_BG_ROWS.BIN: {len(bg_rows)} bytes")

# Write SPAWNS.BIN: uint8 count, then N × (uint16 x_px, uint16 y_px, uint8 type)
spawns_data = bytearray()
spawns_data.append(len(spawns))
for (x_px, y_px, typ) in spawns:
    spawns_data.extend(struct.pack('<HHB', x_px, y_px, typ))

with open(os.path.join(out_root, 'SPAWNS.BIN'), 'wb') as f:
    f.write(spawns_data)
print(f"SPAWNS.BIN: {len(spawns_data)} bytes ({len(spawns)} enemy spawns)")
for i, (x_px, y_px, typ) in enumerate(spawns):
    names = ['Crawler', 'Flyer', 'Turret']
    print(f"  [{i}] {names[typ]:8s}  x={x_px:5d}px  y={y_px:5d}px")

print(f"Player start: ({start_x}, {start_y}) px  — update player_start_x/y in runningman.c if regenerating")
