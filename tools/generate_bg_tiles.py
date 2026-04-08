#!/usr/bin/env python3
"""
generate_bg_tiles.py — Diagonal Circuit Board components.

Provides the 45-degree trace components, vias, and orthogonal IC chips
used by generate_maze.py to route the global circuit board pattern.
"""

import struct
import os

def rgb555(r8, g8, b8):
    """Opaque color: bits [15:11]=B, [10:6]=G, [5]=alpha(1=opaque), [4:0]=R."""
    r = (r8 >> 3) & 0x1F
    g = (g8 >> 3) & 0x1F
    b = (b8 >> 3) & 0x1F
    return (b << 11) | (g << 6) | (1 << 5) | r

PALETTE = [
    rgb555(  0,   0,   0),   # 0  Void Black
    rgb555(  5,   5,  10),   # 1  Deep Substrate Texture
    rgb555(  0,  30,  50),   # 2  Dim Cyan Edge
    rgb555(  0,  80, 120),   # 3  Mid Cyan Line
    rgb555( 40,   0,  50),   # 4  Dim Magenta Edge
    rgb555(100,   0, 120),   # 5  Mid Magenta Line
    rgb555( 10,  10,  15),   # 6  IC Body
    rgb555( 40,  40,  50),   # 7  IC Edge/Highlight
    rgb555(200, 200, 200),   # 8  Silver Pin / Via Core
    rgb555( 20,  30,  50),   # 9  Resistor Body
    rgb555( 50,  80, 120),   # 10 Resistor Band 1
    rgb555(180,  50,  50),   # 11 Resistor Band 2
    rgb555(200, 150,   0),   # 12 Gold Detail
]
# Pad to 16 colors
while len(PALETTE) < 16:
    PALETTE.append(rgb555(0, 0, 0))

def make_tile(rows):
    data = bytearray()
    for row in rows:
        for i in range(0, 8, 2):
            byte = ((row[i] & 0xF) << 4) | (row[i+1] & 0xF)
            data.append(byte)
    return bytes(data)

def solid_tile(c): return make_tile([[c]*8]*8)

def bg_dots():
    return make_tile([
        [1,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,1,0,0,0],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
    ])

def bg_trace_se(E, M):
    return make_tile([
        [M,E,0,0,0,0,0,0],
        [M,M,E,0,0,0,0,0],
        [E,M,M,E,0,0,0,0],
        [0,E,M,M,E,0,0,0],
        [0,0,E,M,M,E,0,0],
        [0,0,0,E,M,M,E,0],
        [0,0,0,0,E,M,M,E],
        [0,0,0,0,0,E,M,M],
    ])

def bg_trace_sw(E, M):
    return make_tile([
        [0,0,0,0,0,0,E,M],
        [0,0,0,0,0,E,M,M],
        [0,0,0,0,E,M,M,E],
        [0,0,0,E,M,M,E,0],
        [0,0,E,M,M,E,0,0],
        [0,E,M,M,E,0,0,0],
        [E,M,M,E,0,0,0,0],
        [M,M,E,0,0,0,0,0],
    ])

def bg_cross(E, M):
    return make_tile([
        [M,E,0,0,0,0,E,M],
        [M,M,E,0,0,E,M,M],
        [E,M,M,E,E,M,M,E],
        [0,E,M,8,8,M,E,0],
        [0,0,E,8,8,E,0,0],
        [0,E,M,8,8,M,E,0],
        [E,M,M,E,E,M,M,E],
        [M,M,E,0,0,E,M,M],
    ])

def bg_via(E, M):
    return make_tile([
        [0,0,0,8,8,0,0,0],
        [0,E,M,8,8,M,E,0],
        [0,M,8,8,8,8,M,0],
        [8,8,8,0,0,8,8,8],
        [8,8,8,0,0,8,8,8],
        [0,M,8,8,8,8,M,0],
        [0,E,M,8,8,M,E,0],
        [0,0,0,8,8,0,0,0],
    ])

def bg_ic_edge_n():
    return make_tile([
        [0,0,0,8,8,0,0,0],
        [0,0,0,8,8,0,0,0],
        [0,0,0,8,8,0,0,0],
        [7,7,7,8,8,7,7,7],
        [6,6,6,6,6,6,6,6],
        [6,6,6,6,6,6,6,6],
        [6,6,6,6,6,6,6,6],
        [6,6,6,6,6,6,6,6],
    ])

def bg_ic_edge_s():
    return make_tile([
        [6,6,6,6,6,6,6,6],
        [6,6,6,6,6,6,6,6],
        [6,6,6,6,6,6,6,6],
        [6,6,6,6,6,6,6,6],
        [7,7,7,8,8,7,7,7],
        [0,0,0,8,8,0,0,0],
        [0,0,0,8,8,0,0,0],
        [0,0,0,8,8,0,0,0],
    ])

def bg_ic_edge_e():
    return make_tile([
        [6,6,6,6,7,0,0,0],
        [6,6,6,6,7,0,0,0],
        [6,6,6,6,7,0,0,0],
        [6,6,6,6,8,8,8,8],
        [6,6,6,6,8,8,8,8],
        [6,6,6,6,7,0,0,0],
        [6,6,6,6,7,0,0,0],
        [6,6,6,6,7,0,0,0],
    ])

def bg_ic_edge_w():
    return make_tile([
        [0,0,0,7,6,6,6,6],
        [0,0,0,7,6,6,6,6],
        [0,0,0,7,6,6,6,6],
        [8,8,8,8,6,6,6,6],
        [8,8,8,8,6,6,6,6],
        [0,0,0,7,6,6,6,6],
        [0,0,0,7,6,6,6,6],
        [0,0,0,7,6,6,6,6],
    ])

def bg_ic_corner_nw():
    return make_tile([
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,7,7,7,7],
        [0,0,0,7,6,6,6,6],
        [0,0,7,6,6,6,6,6],
        [0,0,7,6,6,6,6,6],
        [0,0,7,6,6,6,6,6],
        [0,0,7,6,6,6,6,6],
    ])

def bg_ic_corner_ne():
    return make_tile([
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [7,7,7,7,0,0,0,0],
        [6,6,6,6,7,0,0,0],
        [6,6,6,6,6,7,0,0],
        [6,6,6,6,6,7,0,0],
        [6,6,6,6,6,7,0,0],
        [6,6,6,6,6,7,0,0],
    ])

def bg_ic_corner_sw():
    return make_tile([
        [0,0,7,6,6,6,6,6],
        [0,0,7,6,6,6,6,6],
        [0,0,7,6,6,6,6,6],
        [0,0,7,6,6,6,6,6],
        [0,0,0,7,6,6,6,6],
        [0,0,0,0,7,7,7,7],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
    ])

def bg_ic_corner_se():
    return make_tile([
        [6,6,6,6,6,7,0,0],
        [6,6,6,6,6,7,0,0],
        [6,6,6,6,6,7,0,0],
        [6,6,6,6,6,7,0,0],
        [6,6,6,6,7,0,0,0],
        [7,7,7,7,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
    ])

def bg_res_h():
    return make_tile([
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
        [8,8,9,10,11,9,8,8],
        [8,8,9,10,11,9,8,8],
        [8,8,9,10,11,9,8,8],
        [8,8,9,10,11,9,8,8],
        [0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0],
    ])

def bg_res_v():
    return make_tile([
        [0,0,8,8,8,8,0,0],
        [0,0,8,8,8,8,0,0],
        [0,0,9,9,9,9,0,0],
        [0,0,10,10,10,10,0,0],
        [0,0,11,11,11,11,0,0],
        [0,0,9,9,9,9,0,0],
        [0,0,8,8,8,8,0,0],
        [0,0,8,8,8,8,0,0],
    ])

tiles = [solid_tile(0)] * 256
tiles[1]  = bg_dots()
tiles[2]  = bg_trace_se(2, 3)
tiles[3]  = bg_trace_sw(2, 3)
tiles[4]  = bg_cross(2, 3)
tiles[5]  = bg_via(2, 3)
tiles[6]  = bg_trace_se(4, 5)
tiles[7]  = bg_trace_sw(4, 5)
tiles[8]  = bg_cross(4, 5)
tiles[9]  = bg_via(4, 5)

tiles[10] = solid_tile(6)
tiles[11] = bg_ic_edge_n()
tiles[12] = bg_ic_edge_s()
tiles[13] = bg_ic_edge_e()
tiles[14] = bg_ic_edge_w()
tiles[15] = bg_ic_corner_nw()
tiles[16] = bg_ic_corner_ne()
tiles[17] = bg_ic_corner_sw()
tiles[18] = bg_ic_corner_se()

tiles[19] = bg_res_h()
tiles[20] = bg_res_v()

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
