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
  1-10      = solid wall / platform (blocks player)
  107-110   = ladder tiles (climbable shafts)

BG tile meanings (generate_bg_tiles.py):
  0-7       = background cave planes
  8-15      = stalactites, stalagmites, moss, crystals
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
TILE_SOLID       = 1    # generic solid (for borders/fill)
TILE_PLATFORM_A  = 3    # platform top variant (tile index 3 in STONE_TILES set)
TILE_PLATFORM_B  = 4    # ground fill variant
TILE_LADDER_TOP  = 107
TILE_LADDER_MID1 = 108
TILE_LADDER_MID2 = 109
TILE_LADDER_BOT  = 110

# BG tile IDs
BG_VOID         = 1    # basic dark cave background
BG_MID          = 2    # slightly lighter cave
BG_LIGHT        = 3    # lightest cave
BG_ROCK_DARK    = 4    # dark rock wall texture
BG_ROCK_MID     = 5    # mid rock
BG_DOTS         = 6    # dotted texture
BG_STRIPE       = 7    # diagonal stripe
BG_STAL_BODY    = 8    # stalactite body
BG_STAL_TIP     = 9    # stalactite tip
BG_STAG_BODY    = 10   # stalagmite body
BG_STAG_TIP     = 11   # stalagmite tip
BG_MOSS         = 12   # moss patch
BG_WATER        = 13   # water shimmer
BG_CRYSTAL      = 14   # blue crystal
BG_GLOW         = 15   # purple glow

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
    # 1. Background fill — layered cave atmosphere.
    # ------------------------------------------------------------------
    # Zone rows: the world is split into 3 vertical zones for BG variety.
    zone_boundaries = [WORLD_H // 3, 2 * WORLD_H // 3]  # rows 200, 400

    for col in range(WORLD_W):
        for row in range(WORLD_H):
            if row < zone_boundaries[0]:
                # Upper zone: lighter cave (sky is above the labyrinth)
                t = BG_MID if (col + row) % 7 != 0 else BG_LIGHT
            elif row < zone_boundaries[1]:
                # Middle zone: standard cave darkness
                t = BG_VOID if (col * 3 + row) % 11 != 0 else BG_DOTS
            else:
                # Lower zone: deep cave — rock textures
                t = BG_ROCK_DARK if (col + row * 2) % 9 != 0 else BG_ROCK_MID
            bg_set(col, row, t)

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
    # 4. Ground fill: solid from floor[0] downward (gravel, earth).
    # ------------------------------------------------------------------
    ground_fill_tile = TILE_PLATFORM_B   # tile index 4 = stone_tile_d
    for col in range(BORDER_COLS, WORLD_W - BORDER_COLS):
        for row in range(floor_row[0], WORLD_H - 1):
            fg_set(col, row, ground_fill_tile)
        # BG: show deep rock texture at ground level
        for row in range(floor_row[0], WORLD_H - 1):
            bg_set(col, row, BG_ROCK_DARK)

    # ------------------------------------------------------------------
    # 5. Floor lines for floors 1..NUM_FLOORS-1.
    # ------------------------------------------------------------------
    # For each floor f, store gap locations so ladders can avoid them.
    gap_cols = [[] for _ in range(NUM_FLOORS)]   # list of (start_col, end_col) pairs

    for f in range(1, NUM_FLOORS):
        r = floor_row[f]
        if r < 2 or r >= WORLD_H - 1:
            continue  # floor out of world bounds

        platform_tile = TILE_PLATFORM_A   # tile 3 = stone_tile_c (bevelled top)
        # Choose a BG row above each floor to place a stalactite occasionally
        bg_row_above = r - 1

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

        # Occasionally add stalagmites to BG just above floor lines
        for col in range(BORDER_COLS + 2, WORLD_W - BORDER_COLS - 2, 7 + rng_mod(5)):
            in_gap = any(gs <= col < ge for gs, ge in gaps_placed)
            if not in_gap and fg_get(col, bg_row_above) == TILE_EMPTY:
                bg_set(col, bg_row_above, BG_STAG_TIP)

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
    # 7. BG stalactites sprinkled near ceilings (upper side of low-row flats).
    # ------------------------------------------------------------------
    for f in range(1, NUM_FLOORS):
        r = floor_row[f]
        if r < 3:
            continue
        # Ceiling side of this floor: rows r+1 (just below the solid floor)
        for col in range(BORDER_COLS + 3, WORLD_W - BORDER_COLS - 3, 9 + rng_mod(7)):
            # Only put stalactite where there is no solid FG above
            if fg_get(col, r - 1) == TILE_EMPTY and bg[col * WORLD_H + (r - 1)] < BG_STAL_BODY:
                # Drop a 1-2 tile stalactite from the floor underside
                bg_set(col, r + 1, BG_STAL_BODY)
                bg_set(col, r + 2, BG_STAL_TIP)

    # ------------------------------------------------------------------
    # 8. BG Decorations: crystals, water, moss, glows — random spots.
    # ------------------------------------------------------------------
    seed_rng(0xCAFE)   # fresh seed for decorations
    for _ in range(400):
        col = BORDER_COLS + rng_mod(WORLD_W - 2 * BORDER_COLS)
        row = 2 + rng_mod(WORLD_H - 4)
        choice = rng_mod(5)
        if fg_get(col, row) != TILE_EMPTY:
            continue
        if   choice == 0: t = BG_MOSS
        elif choice == 1: t = BG_CRYSTAL
        elif choice == 2: t = BG_GLOW
        elif choice == 3: t = BG_WATER
        else:             t = BG_MID
        bg_set(col, row, t)

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
