#!/usr/bin/env python3
"""
generate_fg_tiles.py — FG tileset for RPMegaRaider.

Tile organisation (zone matched to generate_maze.py):
  0        — transparent (empty air)
  1-10     — DEEP  zone (world rows 400-600): volcanic basalt, dark rock
  11-20    — MID   zone (world rows 200-400): dungeon brick / standard stone
  21-30    — UPPER zone (world rows   0-200): ancient carved stone + crystal
  107-110  — Ladder tiles (all zones)
"""

import struct, os

def rgb555(r8, g8, b8):
    """Opaque color: bits [15:11]=B, [10:6]=G, [5]=alpha(1=opaque), [4:0]=R."""
    r = (r8 >> 3) & 0x1F
    g = (g8 >> 3) & 0x1F
    b = (b8 >> 3) & 0x1F
    return (b << 11) | (g << 6) | (1 << 5) | r

TRANSPARENT = 0  # alpha=0, all channels 0 — transparent black for FG index 0

PALETTE = [
    TRANSPARENT,             # 0  transparent (FG only: shows BG through)
    rgb555( 15,  15,  20),   # 1  basalt black / deepest shadow
    rgb555( 50,  48,  55),   # 2  dark rock gray
    rgb555( 95,  92, 100),   # 3  medium rock
    rgb555(150, 148, 158),   # 4  light stone
    rgb555(215, 213, 220),   # 5  bright highlight
    rgb555( 45,  30,  18),   # 6  dark earth
    rgb555(120,  88,  55),   # 7  warm stone
    rgb555(165, 132,  90),   # 8  ladder wood / sand
    rgb555(100,  55,  15),   # 9  dark copper ore
    rgb555(185,  95,  35),   # 10 bright copper
    rgb555( 30,  90,  30),   # 11 deep moss
    rgb555(180,  45,  30),   # 12 volcanic red / lava crack
    rgb555( 40, 100, 175),   # 13 crystal blue
    rgb555(140, 190, 240),   # 14 ice blue highlight
    rgb555(255, 240, 160),   # 15 warm gold highlight
]
assert len(PALETTE) == 16

def make_tile(rows):
    assert len(rows) == 8
    data = bytearray()
    for row in rows:
        assert len(row) == 8
        for i in range(0, 8, 2):
            byte = ((row[i] & 0xF) << 4) | (row[i+1] & 0xF)
            data.append(byte)
    return bytes(data)

def solid_tile(c):
    return make_tile([[c]*8]*8)

T = 0   # transparent
W = 5   # bright highlight (white)
Y = 15  # warm gold (yellow)
H = 13  # crystal blue (highlight)
G = 11  # deep moss (green)
B = 14  # ice blue
R = 12  # volcanic red

# =============================================================================
# DEEP ZONE tiles 1-10  —  volcanic basalt, dark rock
# =============================================================================

def deep_basalt():
    """Rough basalt block — main deep wall tile."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,2,2,2,2,2,2,1],
        [1,2,3,2,2,2,2,1],
        [1,2,2,2,3,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,3,2,2,2,3,1],
        [1,2,2,2,2,2,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def deep_basalt_cracked():
    """Basalt with a diagonal crack."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,2,2,1,2,2,2,1],
        [1,2,1,2,2,2,2,1],
        [1,2,2,2,1,2,2,1],
        [1,2,2,2,2,1,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def deep_platform_top():
    """Dark platform — single row top highlight."""
    return make_tile([
        [3,3,3,3,3,3,3,3],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,1,2,2,2,2,1,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def deep_fill_a():
    """Ground fill — deep zone."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,1,2,2,2,2,1,2],
        [2,2,2,2,2,2,2,2],
        [2,2,2,1,2,2,2,2],
        [2,2,2,2,2,1,2,2],
        [2,1,2,2,2,2,2,2],
        [2,2,2,2,2,2,2,1],
        [2,2,2,2,2,2,2,2],
    ])

def deep_volcanic():
    """Basalt with lava crack veins."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,2,2,2,2,2,2,1],
        [1,2,12,12,2,2,2,1],
        [1,2,2,12,2,2,2,1],
        [1,2,2,2,12,12,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def deep_ore_vein():
    """Rock with copper ore veins."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,2,2,10,2,2,2,1],
        [1,2,2,2,10,2,2,1],
        [1,2,9,2,2,2,2,1],
        [1,2,2,2,2,10,2,1],
        [1,2,2,10,2,2,2,1],
        [1,2,2,2,2,2,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def deep_dense_basalt():
    """Dense dark basalt, subtle texture."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,1,2,1,1,2,1,1],
        [1,2,2,2,2,2,2,1],
        [1,1,2,2,2,2,1,1],
        [1,2,2,2,2,2,2,1],
        [1,1,2,1,2,2,1,1],
        [1,2,2,2,2,2,2,1],
        [1,1,1,1,1,1,1,1],
    ])

def deep_fill_strata():
    """Fill with horizontal rock strata lines."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [2,2,2,2,2,2,2,2],
        [2,2,2,2,2,2,2,2],
        [1,1,1,1,1,1,1,1],
        [2,2,2,2,2,2,2,2],
        [2,2,2,2,2,2,2,2],
        [1,1,1,1,1,1,1,1],
        [2,2,2,2,2,2,2,2],
    ])

def deep_lava_fill():
    """Fill with scattered lava glow."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,2,2,1,2,2,2,2],
        [2,1,2,2,2,2,1,2],
        [2,2,2,2,12,2,2,2],
        [2,2,12,2,2,2,2,2],
        [2,2,2,2,2,2,1,2],
        [2,12,2,2,2,2,2,2],
        [2,2,2,2,2,12,2,2],
    ])

def deep_half_platform():
    """Half-height platform — deep zone."""
    return make_tile([
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [3,3,3,3,3,3,3,3],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,1,1,1,1,1,1,1],
    ])

# =============================================================================
# MID ZONE tiles 11-20  —  dungeon brick / standard stone
# =============================================================================

def mid_brick_a():
    """Standard dungeon brick with mortar lines."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,4,3,3,3,3,3,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
        [2,3,3,3,4,3,3,2],
        [2,3,3,3,3,3,3,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def mid_brick_cracked():
    """Brick with a vertical crack."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,4,3,2,3,3,3,2],
        [2,3,3,2,3,3,3,2],
        [2,2,2,2,2,2,2,2],
        [2,3,3,3,2,3,3,2],
        [2,3,3,3,2,3,4,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def mid_platform_top():
    """Platform top — mid zone, bevelled edge."""
    return make_tile([
        [4,4,4,4,4,4,4,4],
        [2,4,3,3,3,3,4,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
        [2,3,3,3,4,3,3,2],
        [2,3,3,3,3,3,3,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def mid_fill_a():
    """Standard fill — mid zone."""
    return make_tile([
        [3,3,3,3,3,3,3,3],
        [3,3,3,3,3,3,3,3],
        [3,2,3,3,3,2,3,3],
        [3,3,3,3,3,3,3,3],
        [3,3,3,2,3,3,3,3],
        [3,3,3,3,3,3,2,3],
        [3,3,2,3,3,3,3,3],
        [3,3,3,3,3,2,3,3],
    ])

def mid_mossy_brick():
    """Brick with moss patches."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,11,11,3,3,3,3,2],
        [2,11,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
        [2,3,3,3,3,11,11,2],
        [2,3,3,3,3,11,3,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def mid_carved():
    """Bevelled carved block."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,5,4,4,4,4,3,2],
        [2,4,3,3,3,3,3,2],
        [2,4,3,3,3,3,3,2],
        [2,4,3,3,3,3,3,2],
        [2,4,3,3,3,3,3,2],
        [2,3,3,3,3,3,2,2],
        [2,2,2,2,2,2,2,2],
    ])

def mid_platform_mossy():
    """Mossy platform top — mid zone."""
    return make_tile([
        [11,4,11,4,11,4,11,4],
        [2,11,3,3,3,3,11,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
        [2,3,3,3,4,3,3,2],
        [2,3,3,3,3,3,3,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def mid_fill_b():
    """Fill with offset mortar grid."""
    return make_tile([
        [3,2,3,3,2,3,3,2],
        [3,3,3,3,3,3,3,3],
        [2,3,3,2,3,3,2,3],
        [3,3,3,3,3,3,3,3],
        [3,2,3,3,2,3,3,2],
        [3,3,3,3,3,3,3,3],
        [2,3,3,2,3,3,2,3],
        [3,3,3,3,3,3,3,3],
    ])

def mid_fill_c():
    """Fill with horizontal strata highlight."""
    return make_tile([
        [3,3,3,3,3,3,3,3],
        [3,3,3,3,3,3,3,3],
        [2,2,2,2,2,2,2,2],
        [3,4,3,3,3,3,4,3],
        [3,3,3,3,3,3,3,3],
        [3,3,3,3,3,3,3,3],
        [2,2,2,2,2,2,2,2],
        [3,3,3,3,3,3,3,3],
    ])

def mid_half_platform():
    """Half-height platform — mid zone."""
    return make_tile([
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [5,4,5,4,5,4,5,4],
        [2,4,3,3,3,3,4,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

# =============================================================================
# UPPER ZONE tiles 21-30  —  ancient carved stone + crystal
# =============================================================================

def upper_carved_a():
    """Ancient carved stone, fine chisel work."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,5,4,4,4,4,4,2],
        [2,4,3,3,3,3,4,2],
        [2,4,3,14,3,3,4,2],
        [2,4,3,3,3,3,4,2],
        [2,4,4,3,3,3,4,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def upper_crystal_vein():
    """Stone with blue crystal veins."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,4,4,13,4,4,4,2],
        [2,4,3,3,13,3,4,2],
        [2,4,3,3,3,3,4,2],
        [2,4,3,3,3,13,4,2],
        [2,4,13,3,3,3,4,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def upper_platform_top():
    """Ancient platform — crystal-edged top."""
    return make_tile([
        [14,13,14,13,14,13,14,13],
        [2,5,4,4,4,4,5,2],
        [2,4,3,3,3,3,4,2],
        [2,4,3,3,3,3,4,2],
        [2,4,3,3,3,3,4,2],
        [2,4,3,3,3,3,3,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def upper_fill_a():
    """Light fill — upper zone."""
    return make_tile([
        [4,4,4,4,4,4,4,4],
        [4,4,4,4,4,4,4,4],
        [4,3,4,4,4,3,4,4],
        [4,4,4,4,4,4,4,4],
        [4,4,4,3,4,4,4,4],
        [4,4,4,4,4,4,3,4],
        [4,3,4,4,4,4,4,4],
        [4,4,4,4,4,3,4,4],
    ])

def upper_crystal_cluster():
    """Crystal cluster embedded in stone face."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,4,4,4,4,4,4,2],
        [2,4,14,13,14,3,4,2],
        [2,4,13,14,13,3,4,2],
        [2,4,14,13,14,3,4,2],
        [2,4,3,3,3,3,4,2],
        [2,3,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def upper_ornate():
    """Ornate inlaid block."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,5,5,5,5,5,5,2],
        [2,5,4,4,4,4,5,2],
        [2,5,4,3,3,4,5,2],
        [2,5,4,3,3,4,5,2],
        [2,5,4,4,4,4,5,2],
        [2,5,5,5,5,5,5,2],
        [2,2,2,2,2,2,2,2],
    ])

def upper_platform_alt():
    """Ancient platform variant — full border."""
    return make_tile([
        [5,5,5,5,5,5,5,5],
        [2,5,4,4,4,4,5,2],
        [2,4,3,3,3,3,4,2],
        [2,4,3,3,3,3,4,2],
        [2,2,2,2,2,2,2,2],
        [2,4,3,3,3,3,4,2],
        [2,4,3,3,3,3,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def upper_fill_b():
    """Light stone fill with bright specks."""
    return make_tile([
        [4,4,4,4,4,4,4,4],
        [4,4,4,4,4,4,4,4],
        [4,5,4,4,4,5,4,4],
        [4,4,4,4,4,4,4,4],
        [4,4,4,4,4,4,4,4],
        [4,5,4,4,4,4,4,5],
        [4,4,4,4,4,4,4,4],
        [4,4,4,4,5,4,4,4],
    ])

def upper_fill_crystal():
    """Light fill with crystal flecks."""
    return make_tile([
        [4,4,4,4,4,4,4,4],
        [4,4,13,4,4,4,4,4],
        [4,4,4,14,4,4,4,4],
        [4,4,4,4,14,4,4,4],
        [4,4,4,4,4,13,4,4],
        [4,4,4,4,4,4,4,4],
        [4,4,13,4,4,4,13,4],
        [4,4,4,4,4,4,4,4],
    ])

def upper_half_platform():
    """Half-height platform — upper zone, crystal rim."""
    return make_tile([
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [14,13,14,13,14,13,14,13],
        [2,5,4,4,4,4,5,2],
        [2,4,3,3,3,3,4,2],
        [2,2,2,2,2,2,2,2],
    ])

# =============================================================================
# Ladder tiles 107-110 (shared across all zones)
# =============================================================================

def ladder_top():
    return make_tile([
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [8,8,8,9,9,8,8,8],
        [9,9,10,9,9,10,9,9],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
    ])

def ladder_mid1():
    return make_tile([
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [8,8,8,9,9,8,8,8],
        [9,9,10,9,9,10,9,9],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
    ])

def ladder_mid2():
    return make_tile([
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [8,8,8,9,9,8,8,8],
        [9,9,10,9,9,10,9,9],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
    ])

def ladder_bot():
    return make_tile([
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [8,8,8,9,9,8,8,8],
        [9,9,10,9,9,10,9,9],
        [T,T,8,9,9,8,T,T],
        [T,T,8,9,9,8,T,T],
        [T,T,T,T,T,T,T,T],
    ])

# =============================================================================
# Pickup tiles 31-33  —  collectibles (not solid — not in TILE_SOLID range)
# =============================================================================

def pickup_charge_pack():
    """Gold lightning bolt on transparent background — collect to gain an EMP charge."""
    G = 15   # warm gold
    C = 10   # bright copper
    T = 0    # transparent
    return make_tile([
        [T, T, T, T, T, T, T, T],
        [T, T, G, G, G, T, T, T],
        [T, G, G, G, T, T, T, T],
        [G, G, C, T, T, T, T, T],
        [T, T, C, C, C, T, T, T],
        [T, T, T, C, C, G, T, T],
        [T, T, T, T, G, G, G, T],
        [T, T, T, T, T, T, T, T],
    ])

def pickup_memory_shard():
    """Bright blue crystal fragment — collect 5 to activate the terminus."""
    B = 13   # crystal blue
    H = 14   # ice blue highlight
    T = 0    # transparent
    return make_tile([
        [T, T, T, H, T, T, T, T],
        [T, T, H, B, H, T, T, T],
        [T, H, B, H, B, H, T, T],
        [H, B, H, H, H, B, H, T],
        [T, H, B, H, B, H, T, T],
        [T, T, H, B, H, T, T, T],
        [T, T, T, H, T, T, T, T],
        [T, T, T, T, T, T, T, T],
    ])

def pickup_terminus():
    """Bright exit beacon — reach this with all 5 shards to escape."""
    Y = 15   # warm gold outer ring
    R = 12   # lava red inner ring
    W = 5    # bright highlight core
    T = 0    # transparent
    return make_tile([
        [T, Y, Y, Y, Y, Y, Y, T],
        [Y, R, R, R, R, R, Y, T],
        [Y, R, W, W, W, R, Y, T],
        [Y, R, W, T, W, R, Y, T],
        [Y, R, W, W, W, R, Y, T],
        [Y, R, R, R, R, R, Y, T],
        [T, Y, Y, Y, Y, Y, Y, T],
        [T, T, T, T, T, T, T, T],
    ])

# =============================================================================
# Portal archway tiles 34-49 — 4×4 grid of 8×8 tiles forming a 32×32 arch
# Placed at entrance (ground floor) and exit (top floor) as decoration.
#
# Palette:  T=0  W=5  Y=15  H=13  G=11  B=14  R=12
#
# Grid layout (tile IDs):
#   34 35 36 37     row 0 (top of arch)
#   38 39 40 41     row 1
#   42 43 44 45     row 2
#   46 47 48 49     row 3 (base)
# =============================================================================

def portal_00():
    T=0
    return make_tile([[T]*8]*8)

def portal_01():
    W=5; Y=15; T=0
    return make_tile([
        [T,T,T,T,W,W,W,W],
        [T,T,T,T,W,W,W,W],
        [T,T,T,W,W,W,W,W],
        [T,T,T,W,W,W,W,W],
        [T,T,Y,Y,Y,Y,Y,Y],
        [T,T,Y,Y,Y,Y,Y,Y],
        [T,Y,Y,Y,Y,W,W,W],
        [T,Y,Y,Y,Y,W,W,W],
    ])

def portal_02():
    W=5; Y=15; T=0
    return make_tile([
        [W,W,W,W,T,T,T,T],
        [W,W,W,W,T,T,T,T],
        [W,W,W,W,W,T,T,T],
        [W,W,W,W,W,T,T,T],
        [Y,Y,Y,Y,Y,Y,T,T],
        [Y,Y,Y,Y,Y,Y,T,T],
        [W,W,W,Y,Y,Y,Y,T],
        [W,W,W,Y,Y,Y,Y,T],
    ])

def portal_03():
    T=0
    return make_tile([[T]*8]*8)

def portal_10():
    W=5; H=13; G=11; B=14; T=0
    return make_tile([
        [W,Y,Y,Y,W,W,W,W],
        [W,Y,Y,Y,W,W,W,W],
        [H,H,B,B,W,W,W,W],
        [H,H,B,B,W,W,W,W],
        [H,H,H,G,G,B,B,B],
        [H,H,H,G,G,B,B,B],
        [H,H,H,H,G,G,G,B],
        [H,H,H,H,G,G,G,B],
    ])

def portal_11():
    W=5; Y=15; H=13; G=11; B=14; T=0
    return make_tile([
        [W,W,W,W,Y,Y,Y,W],
        [W,W,W,W,Y,Y,Y,W],
        [W,W,W,W,B,B,H,H],
        [W,W,W,W,B,B,H,H],
        [B,B,B,B,G,G,H,H],
        [B,B,B,B,G,G,H,H],
        [B,B,B,B,G,G,G,H],
        [B,B,B,B,G,G,G,H],
    ])

def portal_12():
    H=13; T=0
    return make_tile([
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
        [H,T,T,T,T,T,T,T],
        [H,T,T,T,T,T,T,T],
        [H,H,T,T,T,T,T,T],
        [H,H,T,T,T,T,T,T],
    ])

def portal_13():
    T=0
    return make_tile([[T]*8]*8)

def portal_20():
    H=13; G=11; T=0
    return make_tile([
        [H,H,H,H,H,G,G,G],
        [H,H,H,H,H,G,G,G],
        [H,H,H,H,H,G,G,G],
        [H,H,H,H,H,G,G,G],
        [H,H,H,H,H,H,G,G],
        [H,H,H,H,H,H,G,G],
        [H,H,H,H,H,H,H,G],
        [H,H,H,H,H,H,H,G],
    ])

def portal_21():
    G=11; B=14; T=0
    return make_tile([
        [G,G,B,B,B,B,G,G],
        [G,G,B,B,B,B,G,G],
        [G,G,G,B,B,B,B,G],
        [G,G,G,B,B,B,B,G],
        [G,G,G,G,B,B,G,G],
        [G,G,G,G,B,B,G,G],
        [G,G,G,G,G,B,B,G],
        [G,G,G,G,G,B,B,G],
    ])

def portal_22():
    H=13; G=11; T=0
    return make_tile([
        [G,G,H,H,H,H,H,H],
        [G,G,H,H,H,H,H,H],
        [G,G,G,H,H,H,H,H],
        [G,G,G,H,H,H,H,H],
        [G,G,G,G,H,H,H,H],
        [G,G,G,G,H,H,H,H],
        [G,G,G,G,H,H,H,H],
        [G,G,G,G,H,H,H,H],
    ])

def portal_23():
    H=13; T=0
    return make_tile([
        [H,T,T,T,T,T,T,T],
        [H,T,T,T,T,T,T,T],
        [H,H,T,T,T,T,T,T],
        [H,H,T,T,T,T,T,T],
        [H,H,H,T,T,T,T,T],
        [H,H,H,T,T,T,T,T],
        [H,H,H,H,T,T,T,T],
        [H,H,H,H,T,T,T,T],
    ])

def portal_30():
    H=13; G=11; T=0
    return make_tile([
        [H,H,H,H,H,H,H,H],
        [H,H,H,H,H,H,H,H],
        [T,G,G,G,G,G,G,G],
        [T,T,G,G,G,G,G,G],
        [T,T,T,G,G,G,G,G],
        [T,T,T,T,G,G,G,G],
        [T,T,T,T,T,T,G,G],
        [T,T,T,T,T,T,T,T],
    ])

def portal_31():
    H=13; G=11; B=14; R=12; T=0
    return make_tile([
        [H,H,H,G,G,G,G,G],
        [H,H,H,H,G,G,G,G],
        [G,G,G,H,H,B,R,R],
        [G,G,H,H,H,B,B,B],
        [G,H,H,H,H,B,R,R],
        [H,H,H,H,H,B,B,B],
        [G,H,H,H,H,H,B,R],
        [G,G,G,G,H,H,H,H],
    ])

def portal_32():
    H=13; G=11; B=14; R=12; T=0
    return make_tile([
        [G,G,G,G,G,H,H,H],
        [G,G,G,G,H,H,H,H],
        [B,H,H,G,G,G,G,G],
        [B,H,H,H,G,G,G,G],
        [B,H,H,H,H,G,G,G],
        [B,H,H,H,H,H,G,G],
        [R,B,H,H,H,H,H,G],
        [B,B,H,H,H,H,G,G],
    ])

def portal_33():
    H=13; G=11; T=0
    return make_tile([
        [H,H,H,H,H,H,H,H],
        [H,H,H,H,H,H,H,H],
        [G,G,G,G,G,G,G,T],
        [G,G,G,G,G,G,T,T],
        [G,G,G,G,G,T,T,T],
        [G,G,G,G,T,T,T,T],
        [G,G,T,T,T,T,T,T],
        [T,T,T,T,T,T,T,T],
    ])

# =============================================================================
# Build 256-tile array
# =============================================================================
tiles = [solid_tile(0)] * 256  # default: transparent empty

# Deep zone: tiles 1-10
tiles[1]  = deep_basalt()
tiles[2]  = deep_basalt_cracked()
tiles[3]  = deep_platform_top()
tiles[4]  = deep_fill_a()
tiles[5]  = deep_volcanic()
tiles[6]  = deep_ore_vein()
tiles[7]  = deep_dense_basalt()
tiles[8]  = deep_fill_strata()
tiles[9]  = deep_lava_fill()
tiles[10] = deep_half_platform()

# Mid zone: tiles 11-20
tiles[11] = mid_brick_a()
tiles[12] = mid_brick_cracked()
tiles[13] = mid_platform_top()
tiles[14] = mid_fill_a()
tiles[15] = mid_mossy_brick()
tiles[16] = mid_carved()
tiles[17] = mid_platform_mossy()
tiles[18] = mid_fill_b()
tiles[19] = mid_fill_c()
tiles[20] = mid_half_platform()

# Upper zone: tiles 21-30
tiles[21] = upper_carved_a()
tiles[22] = upper_crystal_vein()
tiles[23] = upper_platform_top()
tiles[24] = upper_fill_a()
tiles[25] = upper_crystal_cluster()
tiles[26] = upper_ornate()
tiles[27] = upper_platform_alt()
tiles[28] = upper_fill_b()
tiles[29] = upper_fill_crystal()
tiles[30] = upper_half_platform()

# Pickup tiles: 31-33  (not solid — pickups only)
tiles[31] = pickup_charge_pack()
tiles[32] = pickup_memory_shard()
tiles[33] = pickup_terminus()

# Decoration tiles: 34-49  (portal archway, 4×4 grid of 8×8 tiles)
tiles[34] = portal_00()
tiles[35] = portal_01()
tiles[36] = portal_02()
tiles[37] = portal_03()
tiles[38] = portal_10()
tiles[39] = portal_11()
tiles[40] = portal_12()
tiles[41] = portal_13()
tiles[42] = portal_20()
tiles[43] = portal_21()
tiles[44] = portal_22()
tiles[45] = portal_23()
tiles[46] = portal_30()
tiles[47] = portal_31()
tiles[48] = portal_32()
tiles[49] = portal_33()

# Ladder tiles
tiles[107] = ladder_top()
tiles[108] = ladder_mid1()
tiles[109] = ladder_mid2()
tiles[110] = ladder_bot()

# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
out_dir = os.path.dirname(__file__) or '.'
out_root = os.path.join(out_dir, '..', 'images')
os.makedirs(out_root, exist_ok=True)

tile_bin = bytearray()
for t in tiles:
    assert len(t) == 32, f"tile is {len(t)} bytes"
    tile_bin.extend(t)

with open(os.path.join(out_root, 'FG_TILES.BIN'), 'wb') as f:
    f.write(tile_bin)
print(f"FG_TILES.BIN: {len(tile_bin)} bytes")

pal_bin = bytearray()
for c in PALETTE:
    pal_bin.extend(struct.pack('<H', c))

with open(os.path.join(out_root, 'FG_PAL.BIN'), 'wb') as f:
    f.write(pal_bin)
print(f"FG_PAL.BIN: {len(pal_bin)} bytes")

