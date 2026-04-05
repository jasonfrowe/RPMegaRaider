#!/usr/bin/env python3
"""
generate_maze.py — Generate 800×600-tile platformer world for RPMegaRaider.

Produces column-major binary files (800 cols × 600 rows):
  MAZE_FG.BIN   480,000 bytes — collision/FG tile layer
  MAZE_BG.BIN   480,000 bytes — decorative BG tile layer

Column-major layout: byte at offset (col * WORLD_H + row) is tile at (col, row).
This is optimal for the horizontal-scroll streaming engine (1 seek + 1 read per column).

FG tile meanings (match constants.h and generate_fg_tiles.py):
  0         = empty air (transparent, shows BG through)
  1-30      = solid wall / platform (blocks player); zone-specific appearance
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

import random, os

# ---------------------------------------------------------------------------
# World constants
# ---------------------------------------------------------------------------
WORLD_W     = 800   # columns (tiles)
WORLD_H     = 600   # rows (tiles)

NUM_FLOORS      = 30
BASE_SPACING    = 14    # rows between floor lines (112px ≈ half screen — 2–3 floors visible)
FLOOR_OFFSET    = 4     # ±4 row random offset per floor
MIN_FLOOR_GAP   = 6     # minimum rows between adjacent floors
GROUND_ROW      = 560   # row of the ground floor (rows 560..599 are solid fill)
BORDER_COLS     = 2     # solid columns on each world edge

LADDERS_PER_PAIR = 4    # ladder shafts connecting adjacent floor pairs
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
        base = GROUND_ROW - f * BASE_SPACING
        off  = rng_mod(2 * FLOOR_OFFSET + 1) - FLOOR_OFFSET
        row  = base + off
        # Clamp so floors don't overlap
        ceiling = floor_row[f - 1] - MIN_FLOOR_GAP
        row = max(min(row, ceiling), 2)
        floor_row[f] = row

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

    for f in range(NUM_FLOORS - 1):
        r_bot = floor_row[f]
        r_top = floor_row[f + 1]
        if r_top < 2 or r_bot >= WORLD_H - 1 or r_bot <= r_top:
            continue

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

            # Draw ladder shaft: r_top row (top rung) to r_bot - 1 (bottom rung)
            shaft_top = r_top   # top rung (in / adjacent to upper floor)
            shaft_bot = r_bot - 1  # bottom rung (one row above lower floor)

            # Clear any solid tile at top rung row on the upper floor column
            fg_set(col, shaft_top, TILE_LADDER_TOP)

            for row in range(shaft_top + 1, shaft_bot):
                tile = TILE_LADDER_MID1 if (row & 1) else TILE_LADDER_MID2
                fg_set(col, row, tile)

            fg_set(col, shaft_bot, TILE_LADDER_BOT)

    # ------------------------------------------------------------------
    # 7. BG circuit board components — ICs, LEDs, capacitors.
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
    # 9. Player start position output.
    # ------------------------------------------------------------------
    # Player starts just above floor[1] (one floor up from ground).
    # This places them where they can immediately see the ground floor
    # below AND one or two elevated platforms above.
    start_x = 10 * 8                      # pixel x (tile column 10)
    start_y = (floor_row[1] - 2) * 8     # 2 tiles above floor[1]

    return start_x, start_y

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
start_x, start_y = generate()

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

print(f"Player start: ({start_x}, {start_y}) px  — update player_start_x/y in runningman.c if regenerating")
