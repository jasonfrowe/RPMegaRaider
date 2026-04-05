#!/usr/bin/env python3
"""
generate_fg_tiles.py — Generate FG tileset for RPMegaRaider.

Outputs:
  FG_TILES.BIN  256 tiles × 32 bytes = 8192 bytes
                4bpp "tall" bitmap:  8 rows × 4 bytes/row per tile.
                High nibble = left pixel (pixel 0), low nibble = right pixel (pixel 1).
                Color index 0 is transparent (shows BG layer through).

  FG_PAL.BIN    16 colors × 2 bytes = 32 bytes  (RGB555, little-endian)
"""

import struct, os

# ---------------------------------------------------------------------------
# Palette  (RGB555 little-endian: rrrrrgggggbbbbb across 16 bits)
# ---------------------------------------------------------------------------
def rgb555(r8, g8, b8):
    r = (r8 >> 3) & 0x1F
    g = (g8 >> 3) & 0x1F
    b = (b8 >> 3) & 0x1F
    return (b << 10) | (g << 5) | r   # RP6502 VGA: BGR555 packing

PALETTE = [
    rgb555(  0,   0,   0),   # 0  transparent / black
    rgb555( 24,  24,  24),   # 1  dark outline
    rgb555( 80,  80,  90),   # 2  medium stone gray
    rgb555(140, 140, 155),   # 3  light stone gray
    rgb555(180, 175, 165),   # 4  stone highlight
    rgb555( 60,  45,  30),   # 5  dark earth / shadow
    rgb555(100,  80,  55),   # 6  mid earth
    rgb555(130, 105,  70),   # 7  light earth
    rgb555( 90,  60,  25),   # 8  dark ladder wood
    rgb555(160, 110,  50),   # 9  mid ladder wood
    rgb555(200, 158,  90),   # 10 light ladder wood / rung
    rgb555( 50, 100,  45),   # 11 mossy green
    rgb555(200,  50,  50),   # 12 accent red (hazards)
    rgb555(240, 200,  60),   # 13 gold / treasure accent
    rgb555(100, 140, 200),   # 14 sky blue reflection
    rgb555(255, 255, 255),   # 15 bright white
]

assert len(PALETTE) == 16

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def make_tile(rows):
    """Pack a list of 8 rows (each a list of 8 color indices) into 32 bytes (4bpp)."""
    assert len(rows) == 8
    data = bytearray()
    for row in rows:
        assert len(row) == 8
        for i in range(0, 8, 2):
            byte = ((row[i] & 0xF) << 4) | (row[i+1] & 0xF)
            data.append(byte)
    return bytes(data)

def solid_tile(c):
    """Tile filled entirely with one color index."""
    return make_tile([[c]*8]*8)

def bordered_tile(border, fill):
    """Tile with a 1-pixel border and filled interior."""
    rows = []
    for r in range(8):
        if r == 0 or r == 7:
            rows.append([border]*8)
        else:
            rows.append([border] + [fill]*6 + [border])
    return make_tile(rows)

# ---------------------------------------------------------------------------
# Tile library
# ---------------------------------------------------------------------------
# Stone tiles 1-10  (solid walls / platforms)
STONE_FILL      = 2   # medium gray
STONE_LIGHT     = 3   # lighter gray interior
STONE_HIGHLIGHT = 4   # brightest highlight
STONE_DARK      = 1   # outline

def stone_tile_a():
    """Classic bevelled stone block."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,4,3,3,3,3,2,1],
        [1,3,2,2,2,2,2,1],
        [1,3,2,2,2,2,2,1],
        [1,3,2,2,2,2,2,1],
        [1,3,2,2,2,2,2,1],
        [1,2,2,2,2,2,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def stone_tile_b():
    """Stone with crack."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,3,2,2,3,2,2,1],
        [1,2,2,2,1,2,2,1],
        [1,2,2,1,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,3,2,2,2,2,3,1],
        [1,2,2,2,2,2,2,1],
        [1,1,1,1,1,1,1,1],
    ])

def stone_tile_c():
    """Platform top (lighter on top edge)."""
    return make_tile([
        [4,4,4,4,4,4,4,4],
        [1,3,3,3,3,3,3,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,1,1,1,1,1,1,1],
    ])

def stone_tile_d():
    """Ground fill (no top highlight)."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [1,2,2,2,2,2,2,2],
        [2,2,3,2,2,3,2,2],
        [2,2,2,2,2,2,2,2],
        [2,2,2,3,2,2,2,2],
        [2,2,2,2,2,2,2,2],
        [2,2,3,2,2,2,3,2],
        [2,2,2,2,2,2,2,2],
    ])

def stone_tile_e():
    """Mossy stone."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,11,11,3,3,11,2,1],
        [1,11,2,2,2,2,2,1],
        [1,3,2,2,3,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,3,2,2,3,2,1],
        [1,11,2,11,2,2,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def stone_tile_f():
    """Rough stone (no bevel)."""
    return make_tile([
        [1,2,1,2,2,1,2,1],
        [2,2,2,2,2,2,2,2],
        [2,3,2,2,3,2,2,2],
        [2,2,2,2,2,2,3,2],
        [2,2,3,2,2,2,2,2],
        [2,2,2,2,3,2,2,3],
        [2,3,2,2,2,2,2,2],
        [1,2,2,1,2,1,2,1],
    ])

def stone_tile_g():
    """Dark stone."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,2,2,2,2,2,2,1],
        [1,2,1,2,2,1,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,1,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def stone_tile_h():
    """Platform with moss top."""
    return make_tile([
        [11,4,11,4,11,4,11,4],
        [1,11,3,3,3,3,11,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,3,2,2,3,2,1],
        [1,2,2,2,2,2,2,1],
        [1,2,2,2,2,2,2,1],
        [1,1,1,1,1,1,1,1],
    ])

def stone_tile_i():
    """Ground fill b (variant)."""
    return make_tile([
        [2,2,2,2,2,2,2,2],
        [2,5,2,2,2,2,5,2],
        [2,2,2,5,2,2,2,2],
        [2,2,2,2,2,5,2,2],
        [5,2,2,2,2,2,2,2],
        [2,2,5,2,2,2,2,5],
        [2,2,2,2,5,2,2,2],
        [2,2,2,2,2,2,2,2],
    ])

def stone_tile_j():
    """Half platform (bottom-aligned)."""
    return make_tile([
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [4,4,4,4,4,4,4,4],
        [1,3,3,3,3,3,3,1],
        [1,2,2,2,2,2,2,1],
        [1,1,1,1,1,1,1,1],
    ])

STONE_TILES = [
    stone_tile_a(),   # 1
    stone_tile_b(),   # 2
    stone_tile_c(),   # 3
    stone_tile_d(),   # 4
    stone_tile_e(),   # 5
    stone_tile_f(),   # 6
    stone_tile_g(),   # 7
    stone_tile_h(),   # 8
    stone_tile_i(),   # 9
    stone_tile_j(),   # 10
]

# Ladder tiles 107-110

def ladder_top():
    """Top rung of ladder (connects to floor above)."""
    return make_tile([
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [8,8,8,9,9,8,8,8],
        [9,9,10,9,9,10,9,9],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
    ])

def ladder_mid1():
    """Ladder middle (rung variant 1)."""
    return make_tile([
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [8,8,8,9,9,8,8,8],
        [9,9,10,9,9,10,9,9],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
    ])

def ladder_mid2():
    """Ladder middle (rung variant 2 — offset rung)."""
    return make_tile([
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [8,8,8,9,9,8,8,8],
        [9,9,10,9,9,10,9,9],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
    ])

def ladder_bot():
    """Bottom rung of ladder."""
    return make_tile([
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [8,8,8,9,9,8,8,8],
        [9,9,10,9,9,10,9,9],
        [0,0,8,9,9,8,0,0],
        [0,0,8,9,9,8,0,0],
        [0,0,0,0,0,0,0,0],
    ])

# ---------------------------------------------------------------------------
# Build the 256-tile array
# ---------------------------------------------------------------------------
tiles = [solid_tile(0)] * 256   # default: transparent empty tile

# Tiles 1-10: solid / platform tiles
for i, t in enumerate(STONE_TILES):
    tiles[1 + i] = t

# Tiles 107-110: ladder tiles
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
