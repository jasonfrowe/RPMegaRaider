#!/usr/bin/env python3
"""
generate_bg_tiles.py — Circuit board BG tileset for RPMegaRaider.

Tile index layout (matched to generate_maze.py BG_ constants):
  0   BG_SUBSTRATE      — dark PCB background
  1   BG_SUBSTRATE_ALT  — substrate with faint texture
  2   BG_TRACE_H        — horizontal gold trace
  3   BG_TRACE_V        — vertical gold trace
  4   BG_CORNER_A       — trace corner (right+up)
  5   BG_CORNER_B       — trace corner (left+up)
  6   BG_VIA            — solder via / pad
  7   BG_T_JUNC         — T-junction (H + V stub down)
  8   BG_IC_BODY        — IC chip body
  9   BG_IC_PINS        — IC chip with pin stubs
  10  BG_PAD            — small copper pad
  11  BG_CAPACITOR      — capacitor (blue body)
  12  BG_LED_RED        — red LED indicator
  13  BG_LED_GREEN      — green LED indicator
  14  BG_TRACE_VIA      — H trace with centre via blob
  15  BG_CROSS          — cross junction (H+V, bright centre)

Outputs:
  BG_TILES.BIN  256 tiles × 32 bytes = 8192 bytes
  BG_PAL.BIN    16 colors × 2 bytes  = 32 bytes  (RGB555 LE)
"""

import struct, os

def rgb555(r8, g8, b8):
    """Opaque color: bits [15:11]=B, [10:6]=G, [5]=alpha(1=opaque), [4:0]=R."""
    r = (r8 >> 3) & 0x1F
    g = (g8 >> 3) & 0x1F
    b = (b8 >> 3) & 0x1F
    return (b << 11) | (g << 6) | (1 << 5) | r   # alpha bit always set (BG is never transparent)

# ---------------------------------------------------------------------------
# BG Palette  — circuit board / PCB theme
# ---------------------------------------------------------------------------
PALETTE = [
    rgb555(  6,  12,   6),   # 0  deep PCB black-green (darkest)
    rgb555( 12,  22,  10),   # 1  dark PCB substrate
    rgb555( 20,  35,  15),   # 2  mid substrate
    rgb555( 30,  52,  22),   # 3  lighter substrate area
    rgb555(130,  92,   8),   # 4  dark gold trace edge
    rgb555(195, 150,  18),   # 5  gold trace main
    rgb555(240, 205,  65),   # 6  bright gold / solder highlight
    rgb555(175,  75,  18),   # 7  copper pad dark
    rgb555(218, 155,  75),   # 8  copper pad light
    rgb555( 18,  18,  32),   # 9  IC chip body dark
    rgb555( 32,  32,  58),   # 10 IC chip body mid
    rgb555( 65,  65,  95),   # 11 IC chip highlight
    rgb555(200,  28,  28),   # 12 red LED / power indicator
    rgb555( 28, 195,  75),   # 13 green LED / status indicator
    rgb555( 28,  75, 215),   # 14 blue capacitor / trace
    rgb555(220, 220, 232),   # 15 white silkscreen
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

# Trace occupies rows 3-4 (H) or cols 3-4 (V), 2 pixels wide
# Palette: 1=substrate, 4=trace edge (dark gold), 5=trace main, 6=bright

def bg_substrate():
    """Plain PCB substrate background."""
    return solid_tile(1)

def bg_substrate_alt():
    """Substrate with faint via dots."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,2,1,1,1,2,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,2,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,2,1,1,1,1,1,2],
        [1,1,1,1,1,1,1,1],
    ])

def bg_trace_h():
    """Horizontal gold trace through tile centre (rows 3-4)."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
        [4,5,5,5,5,5,5,4],
        [4,5,5,5,5,5,5,4],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def bg_trace_v():
    """Vertical gold trace through tile centre (cols 3-4)."""
    return make_tile([
        [1,1,1,4,4,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,4,4,1,1,1],
    ])

def bg_corner_a():
    """Corner: V trace from top bends right (SE turn)."""
    return make_tile([
        [1,1,1,4,4,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,5,5,4],
        [1,1,1,4,5,5,5,4],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def bg_corner_b():
    """Corner: V trace from top bends left (SW turn)."""
    return make_tile([
        [1,1,1,4,4,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [4,5,5,5,5,1,1,1],
        [4,5,5,5,4,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def bg_via():
    """Solder via — copper ring with drill hole."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,1,4,5,5,4,1,1],
        [1,4,5,7,7,5,4,1],
        [1,5,7,0,0,7,5,1],
        [1,5,7,0,0,7,5,1],
        [1,4,5,7,7,5,4,1],
        [1,1,4,5,5,4,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def bg_t_junc():
    """T-junction: H trace + V trace going downward."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
        [4,5,5,5,5,5,5,4],
        [4,5,5,5,5,5,5,4],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,4,4,1,1,1],
    ])

def bg_ic_body():
    """IC chip interior body."""
    return make_tile([
        [9,9,9,9,9,9,9,9],
        [9,10,10,10,10,10,10,9],
        [9,10,10,10,10,10,10,9],
        [9,10,11,11,11,11,10,9],
        [9,10,11,11,11,11,10,9],
        [9,10,10,10,10,10,10,9],
        [9,10,10,10,10,10,10,9],
        [9,9,9,9,9,9,9,9],
    ])

def bg_ic_pins():
    """IC chip with gold pin stubs on left side."""
    return make_tile([
        [1,9,9,9,9,9,9,9],
        [5,9,10,10,10,10,10,9],
        [1,9,10,11,11,11,10,9],
        [5,9,10,11,9,11,10,9],
        [5,9,10,11,9,11,10,9],
        [1,9,10,11,11,11,10,9],
        [5,9,10,10,10,10,10,9],
        [1,9,9,9,9,9,9,9],
    ])

def bg_pad():
    """Small square copper component pad."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,4,4,4,4,4,4,1],
        [1,4,5,5,5,5,4,1],
        [1,4,5,6,6,5,4,1],
        [1,4,5,6,6,5,4,1],
        [1,4,5,5,5,5,4,1],
        [1,4,4,4,4,4,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def bg_capacitor():
    """Capacitor — blue cylindrical body with gold leads."""
    return make_tile([
        [1,1,1,4,4,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,14,14,14,14,14,14,1],
        [1,14,15,14,14,15,14,1],
        [1,14,14,14,14,14,14,1],
        [1,14,14,14,14,14,14,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,4,4,1,1,1],
    ])

def bg_led_red():
    """Red LED indicator with gold leads."""
    return make_tile([
        [1,1,1,4,4,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,12,12,12,12,1,1],
        [1,12,12,12,12,12,12,1],
        [1,12,12,12,12,12,12,1],
        [1,1,12,12,12,12,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,4,4,1,1,1],
    ])

def bg_led_green():
    """Green LED indicator with gold leads."""
    return make_tile([
        [1,1,1,4,4,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,13,13,13,13,1,1],
        [1,13,13,13,13,13,13,1],
        [1,13,13,13,13,13,13,1],
        [1,1,13,13,13,13,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,4,4,1,1,1],
    ])

def bg_trace_via():
    """H trace with solder blob at centre."""
    return make_tile([
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,7,7,1,1,1],
        [4,5,5,8,8,5,5,4],
        [4,5,5,8,8,5,5,4],
        [1,1,1,7,7,1,1,1],
        [1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1],
    ])

def bg_cross():
    """Cross junction — H and V traces, bright solder at centre."""
    return make_tile([
        [1,1,1,4,4,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [4,5,5,6,6,5,5,4],
        [4,5,5,6,6,5,5,4],
        [1,1,1,5,5,1,1,1],
        [1,1,1,5,5,1,1,1],
        [1,1,1,4,4,1,1,1],
    ])

# ---------------------------------------------------------------------------
# Build the 256-tile array
# ---------------------------------------------------------------------------
tiles = [solid_tile(0)] * 256   # default: deep PCB void

tiles[0]  = bg_substrate()
tiles[1]  = bg_substrate_alt()
tiles[2]  = bg_trace_h()
tiles[3]  = bg_trace_v()
tiles[4]  = bg_corner_a()
tiles[5]  = bg_corner_b()
tiles[6]  = bg_via()
tiles[7]  = bg_t_junc()
tiles[8]  = bg_ic_body()
tiles[9]  = bg_ic_pins()
tiles[10] = bg_pad()
tiles[11] = bg_capacitor()
tiles[12] = bg_led_red()
tiles[13] = bg_led_green()
tiles[14] = bg_trace_via()
tiles[15] = bg_cross()

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
