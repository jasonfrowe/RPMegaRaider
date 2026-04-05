#!/usr/bin/env python3
"""
generate_bg_tiles.py — Generate BG tileset for RPMegaRaider.

Outputs:
  BG_TILES.BIN  256 tiles × 32 bytes = 8192 bytes
                4bpp "tall" bitmap: high nibble = left pixel, low nibble = right pixel.
                Color index 0 is the "furthest back" color (solid BG color).
                BG is on plane 0 (no transparency needed — fills everything behind FG).

  BG_PAL.BIN    16 colors × 2 bytes = 32 bytes  (RGB555 LE)

The BG layer is purely decorative cave/dungeon scenery viewed behind the FG layer.
"""

import struct, os

def rgb555(r8, g8, b8):
    r = (r8 >> 3) & 0x1F
    g = (g8 >> 3) & 0x1F
    b = (b8 >> 3) & 0x1F
    return (b << 10) | (g << 5) | r   # BGR555 packing

# ---------------------------------------------------------------------------
# BG Palette  (cave / dungeon atmosphere — deep blues and purples)
# ---------------------------------------------------------------------------
PALETTE = [
    rgb555( 12,  10,  20),   # 0  deep cave background (near black / dark blue)
    rgb555( 25,  20,  40),   # 1  dark cave void
    rgb555( 40,  35,  60),   # 2  mid cave shadow
    rgb555( 55,  50,  80),   # 3  lighter cave shadow
    rgb555( 70,  65, 100),   # 4  cave mid-tone
    rgb555( 90,  85, 120),   # 5  cave highlight
    rgb555( 45,  30,  20),   # 6  dark rock/stalagmite
    rgb555( 70,  55,  35),   # 7  mid rock
    rgb555(100,  80,  55),   # 8  light rock
    rgb555( 20,  15,  35),   # 9  deep void accent
    rgb555( 60,  40,  80),   # 10 purple cave accent
    rgb555( 80,  60, 100),   # 11 soft purple highlight
    rgb555( 30,  55,  30),   # 12 deep moss green
    rgb555( 50,  80,  45),   # 13 moss highlight
    rgb555( 15,  45,  60),   # 14 deep water reflection
    rgb555( 30,  70,  90),   # 15 water highlight
]

assert len(PALETTE) == 16

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# BG tile library
# ---------------------------------------------------------------------------

def sky_tile():
    """Open cave void — plain background."""
    return solid_tile(1)

def sky_mid_tile():
    """Slightly lighter cave void."""
    return solid_tile(2)

def sky_light_tile():
    """Lighter background area for depth variation."""
    return solid_tile(3)

def cave_wall_dark():
    """Cave back-wall rock (dark)."""
    return make_tile([
        [1,1,2,2,2,1,1,1],
        [1,2,2,2,2,2,1,1],
        [2,2,6,2,2,2,2,1],
        [2,2,2,2,6,2,2,2],
        [1,2,2,2,2,2,2,2],
        [1,1,2,6,2,2,2,1],
        [1,1,2,2,2,6,2,1],
        [1,1,1,2,2,2,1,1],
    ])

def cave_wall_mid():
    """Cave back-wall rock (mid)."""
    return make_tile([
        [2,2,3,3,3,2,2,2],
        [2,3,3,3,3,3,2,2],
        [3,3,7,3,3,3,3,2],
        [3,3,3,3,7,3,3,3],
        [2,3,3,3,3,3,3,3],
        [2,2,3,7,3,3,3,2],
        [2,2,3,3,3,7,3,2],
        [2,2,2,3,3,3,2,2],
    ])

def stalactite_tip():
    """Stalactite hanging from above — tip only."""
    return make_tile([
        [0,0,6,7,7,6,0,0],
        [0,0,6,7,7,6,0,0],
        [0,0,6,7,7,6,0,0],
        [0,0,0,6,6,0,0,0],
        [0,0,0,6,6,0,0,0],
        [0,0,0,0,6,0,0,0],
        [0,0,0,0,6,0,0,0],
        [0,0,0,0,0,0,0,0],
    ])

def stalactite_body():
    """Stalactite body (connects to ceiling)."""
    return make_tile([
        [0,6,7,7,7,7,6,0],
        [0,6,7,7,7,7,6,0],
        [0,6,7,8,7,7,6,0],
        [0,6,7,7,7,7,6,0],
        [0,0,6,7,7,6,0,0],
        [0,0,6,7,7,6,0,0],
        [0,0,6,7,7,6,0,0],
        [0,0,6,7,7,6,0,0],
    ])

def stalagmite_tip():
    """Stalagmite rising from below — tip only."""
    return make_tile([
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,6,0,0,0],
        [0,0,0,0,6,0,0,0],
        [0,0,0,6,6,0,0,0],
        [0,0,0,6,6,0,0,0],
        [0,0,6,7,7,6,0,0],
        [0,0,6,7,7,6,0,0],
        [0,0,6,7,7,6,0,0],
    ])

def stalagmite_body():
    """Stalagmite body."""
    return make_tile([
        [0,0,6,7,7,6,0,0],
        [0,0,6,7,7,6,0,0],
        [0,0,6,7,7,6,0,0],
        [0,6,7,8,7,7,6,0],
        [0,6,7,7,7,7,6,0],
        [0,6,7,7,7,7,6,0],
        [0,6,7,7,7,7,6,0],
        [0,6,7,7,7,7,6,0],
    ])

def moss_patch():
    """Mossy background detail."""
    return make_tile([
        [1,1,1,12,12,1,1,1],
        [1,1,12,13,13,12,1,1],
        [1,12,13,13,13,13,12,1],
        [1,12,12,13,12,12,12,1],
        [1,1,12,12,12,12,1,1],
        [1,1,1,12,12,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def water_shimmer():
    """Underground water effect."""
    return make_tile([
        [14,14,14,15,14,14,15,14],
        [14,15,14,14,14,15,14,14],
        [15,14,14,14,14,14,14,15],
        [14,14,15,14,15,14,14,14],
        [14,14,14,14,14,14,15,14],
        [14,15,14,15,14,14,14,14],
        [14,14,14,14,14,15,14,15],
        [15,14,15,14,14,14,14,14],
    ])

def crystal_blue():
    """Blue crystal formation."""
    return make_tile([
        [1,1,1,14,15,1,1,1],
        [1,1,14,15,15,14,1,1],
        [1,14,15,15,15,15,14,1],
        [1,14,15,14,14,15,14,1],
        [1,1,14,15,15,14,1,1],
        [1,1,1,14,14,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def purple_glow():
    """Glowing purple mushroom/crystal."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,1,1,10,10,1,1,1],
        [1,1,10,11,11,10,1,1],
        [1,10,11,11,11,11,10,1],
        [1,10,10,11,10,10,10,1],
        [1,1,10,10,10,10,1,1],
        [1,1,1,10,10,1,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def cave_dot_pattern():
    """Subtle dotted cave texture."""
    return make_tile([
        [1,1,1,2,1,1,1,1],
        [1,1,1,1,1,1,2,1],
        [1,2,1,1,1,1,1,1],
        [1,1,1,1,2,1,1,1],
        [1,1,2,1,1,1,1,2],
        [1,1,1,1,1,2,1,1],
        [2,1,1,2,1,1,1,1],
        [1,1,1,1,1,1,2,1],
    ])

def cave_stripe():
    """Diagonal stripe rock texture."""
    return make_tile([
        [2,1,1,1,2,1,1,1],
        [1,2,1,1,1,2,1,1],
        [1,1,2,1,1,1,2,1],
        [1,1,1,2,1,1,1,2],
        [2,1,1,1,2,1,1,1],
        [1,2,1,1,1,2,1,1],
        [1,1,2,1,1,1,2,1],
        [1,1,1,2,1,1,1,2],
    ])

# ---------------------------------------------------------------------------
# Build the 256-tile array
# ---------------------------------------------------------------------------
tiles = [solid_tile(0)] * 256   # default: deep void

# Tile 0: deep background (fill)
tiles[0]  = solid_tile(0)

# Tiles 1-5: cave background variants
tiles[1]  = sky_tile()
tiles[2]  = sky_mid_tile()
tiles[3]  = sky_light_tile()
tiles[4]  = cave_wall_dark()
tiles[5]  = cave_wall_mid()
tiles[6]  = cave_dot_pattern()
tiles[7]  = cave_stripe()

# Tiles 8-11: geological features
tiles[8]  = stalactite_body()
tiles[9]  = stalactite_tip()
tiles[10] = stalagmite_body()
tiles[11] = stalagmite_tip()

# Tiles 12-15: organic / magical features
tiles[12] = moss_patch()
tiles[13] = water_shimmer()
tiles[14] = crystal_blue()
tiles[15] = purple_glow()

# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
out_dir = os.path.dirname(__file__) or '.'
out_root = os.path.join(out_dir, '..', 'images')
os.makedirs(out_root, exist_ok=True)

tile_bin = bytearray()
for t in tiles:
    assert len(t) == 32
    tile_bin.extend(t)

with open(os.path.join(out_root, 'BG_TILES.BIN'), 'wb') as f:
    f.write(tile_bin)
print(f"BG_TILES.BIN: {len(tile_bin)} bytes")

pal_bin = bytearray()
for c in PALETTE:
    pal_bin.extend(struct.pack('<H', c))

with open(os.path.join(out_root, 'BG_PAL.BIN'), 'wb') as f:
    f.write(pal_bin)
print(f"BG_PAL.BIN: {len(pal_bin)} bytes")
