#!/usr/bin/env python3
"""
generate_fg_tiles.py — FG tileset for RPMegaRaider.

Tile organisation (zone matched to generate_maze.py):
  0        — transparent (empty air)
  1-10     — DEEP  zone (world rows 400-600): Classic Brick
  11-20    — MID   zone (world rows 200-400): Classic Brick
  21-30    — UPPER zone (world rows   0-200): Classic Brick
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
    TRANSPARENT,             # 0  Transparent
    rgb555(  0,   0,   0),   # 1  Pitch Black / Mortar
    rgb555( 90,  30,  30),   # 2  Dark Red Brick
    rgb555(180,  50,  40),   # 3  Mid Red Brick
    rgb555(255, 255, 255),   # 4  Pure White (Intense Core)
    rgb555(  0,  60,  80),   # 5  Dim Cyan
    rgb555(  0, 200, 255),   # 6  Bright Cyan
    rgb555( 80,  20,   0),   # 7  Dim Orange
    rgb555(255, 100,   0),   # 8  Bright Orange
    rgb555( 60,   0,  60),   # 9  Dim Magenta
    rgb555(255,   0, 255),   # 10 Bright Magenta
    rgb555(100,  55,  15),   # 11 Dark Wood (Ladder)
    rgb555(165, 132,  90),   # 12 Light Wood (Ladder)
    rgb555(255, 220,   0),   # 13 Gold / Yellow
    rgb555(255,  20,  50),   # 14 Bright Red
    rgb555( 10,  20,  60),   # 15 Dark Blue
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

# =============================================================================
# CLASSIC BRICK PATTERNS
# =============================================================================

def t_solid():
    H = 13; M = 3; D = 2; K = 1
    return make_tile([
        [H, H, H, H, H, H, H, K],
        [M, M, M, M, M, M, M, K],
        [M, M, M, M, M, M, M, K],
        [K, K, K, K, K, K, K, K],
        [H, H, H, K, H, H, H, H],
        [M, M, M, K, M, M, M, M],
        [M, M, M, K, M, M, M, M],
        [K, K, K, K, K, K, K, K],
    ])

def t_platform():
    H = 13; M = 3; D = 2; K = 1
    return make_tile([
        [H, H, H, H, H, H, H, H],
        [H, H, H, H, H, H, H, H],
        [K, K, K, K, K, K, K, K],
        [H, H, H, K, H, H, H, H],
        [M, M, M, K, M, M, M, M],
        [M, M, M, K, M, M, M, M],
        [D, D, D, K, D, D, D, D],
        [K, K, K, K, K, K, K, K],
    ])

def t_fill():
    M = 3; D = 2; K = 1
    return make_tile([
        [M, M, M, M, M, M, M, K],
        [D, D, D, D, D, D, D, K],
        [D, D, D, D, D, D, D, K],
        [K, K, K, K, K, K, K, K],
        [M, M, M, K, M, M, M, M],
        [D, D, D, K, D, D, D, D],
        [D, D, D, K, D, D, D, D],
        [K, K, K, K, K, K, K, K],
    ])

# =============================================================================
# Ladder tiles 107-110 (shared across all zones)
# =============================================================================

def ladder_top():
    T = 0; D = 11; L = 12
    return make_tile([
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [L,L,L,D,D,L,L,L],
        [D,D,D,D,D,D,D,D],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
    ])

def ladder_mid1():
    T = 0; D = 11; L = 12
    return make_tile([
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [L,L,L,D,D,L,L,L],
        [D,D,D,D,D,D,D,D],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
    ])

def ladder_mid2():
    T = 0; D = 11; L = 12
    return make_tile([
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [L,L,L,D,D,L,L,L],
        [D,D,D,D,D,D,D,D],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
    ])

def ladder_bot():
    T = 0; D = 11; L = 12
    return make_tile([
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [L,L,L,D,D,L,L,L],
        [D,D,D,D,D,D,D,D],
        [T,T,L,D,D,L,T,T],
        [T,T,L,D,D,L,T,T],
        [T,T,T,T,T,T,T,T],
    ])

# =============================================================================
# Pickup tiles 31-33  —  collectibles (not solid — not in TILE_SOLID range)
# =============================================================================

def pickup_charge_pack():
    """Electric lightning bolt."""
    Y = 13; W = 4; T = 0
    return make_tile([
        [T, T, T, T, T, T, T, T],
        [T, T, Y, Y, Y, T, T, T],
        [T, Y, Y, Y, T, T, T, T],
        [Y, Y, W, T, T, T, T, T],
        [T, T, W, W, W, T, T, T],
        [T, T, T, W, W, Y, T, T],
        [T, T, T, T, Y, Y, Y, T],
        [T, T, T, T, T, T, T, T],
    ])

def pickup_memory_shard():
    """Cyan neon diamond."""
    C = 6; W = 4; T = 0
    return make_tile([
        [T, T, T, W, T, T, T, T],
        [T, T, W, C, W, T, T, T],
        [T, W, C, W, C, W, T, T],
        [W, C, W, W, W, C, W, T],
        [T, W, C, W, C, W, T, T],
        [T, T, W, C, W, T, T, T],
        [T, T, T, W, T, T, T, T],
        [T, T, T, T, T, T, T, T],
    ])

def pickup_terminus():
    """Glowing exit beacon."""
    Y = 8; R = 14; W = 4; T = 0
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
# Palette:  W=4  Y=13  H=6  G=10  B=15  R=14  T=0
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
    W=4; Y=13; T=0
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
    W=4; Y=13; T=0
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
    W=4; Y=13; H=6; G=10; B=15; T=0
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
    W=4; Y=13; H=6; G=10; B=15; T=0
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
    H=6; T=0
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
    H=6; G=10; T=0
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
    G=10; B=15; T=0
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
    H=6; G=10; T=0
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
    H=6; T=0
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
    H=6; G=10; T=0
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
    H=6; G=10; B=15; R=14; T=0
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
    H=6; G=10; B=15; R=14; T=0
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
    H=6; G=10; T=0
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

# 1. DEEP ZONE: Classic Brick
tiles[1]  = t_solid()
tiles[2]  = t_solid()
tiles[3]  = t_platform()
tiles[4]  = t_fill()
tiles[5]  = t_solid()
tiles[6]  = t_solid()
tiles[7]  = t_solid()
tiles[8]  = t_solid()
tiles[9]  = t_solid()
tiles[10] = t_solid()

# 2. MID ZONE: Classic Brick
tiles[11] = t_solid()
tiles[12] = t_solid()
tiles[13] = t_platform()
tiles[14] = t_fill()
tiles[15] = t_solid()
tiles[16] = t_solid()
tiles[17] = t_solid()
tiles[18] = t_solid()
tiles[19] = t_solid()
tiles[20] = t_solid()

# 3. UPPER ZONE: Classic Brick
tiles[21] = t_solid()
tiles[22] = t_solid()
tiles[23] = t_platform()
tiles[24] = t_fill()
tiles[25] = t_solid()
tiles[26] = t_solid()
tiles[27] = t_solid()
tiles[28] = t_solid()
tiles[29] = t_solid()
tiles[30] = t_solid()

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
