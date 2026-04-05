#!/usr/bin/env python3
"""
generate_enemy_sprites.py — Enemy sprite sheet for RPMegaRaider.

Produces images/EnemySprites.bin  (6 frames × 16×16 × 2 bytes = 3072 bytes)

Each pixel is stored as little-endian RGB555:
  bit[15:11]=B  bit[10:6]=G  bit[5]=alpha(1=opaque)  bit[4:0]=R
  0x0000 = fully transparent

Frame layout:
  0 — Crawler walk A  (PCB bug, legs extended left/right)
  1 — Crawler walk B  (PCB bug, legs in alternate position)
  2 — Flyer bob A     (glitchy data packet, position high)
  3 — Flyer bob B     (glitchy data packet, position low)
  4 — Turret idle     (capacitor-body sentinel, dim LED)
  5 — Turret armed    (bright LED + spark, player nearby)
"""

import struct, os

FRAME_SIZE = 16 * 16 * 2   # 512 bytes per frame

def rgb(r8, g8, b8):
    """Opaque RGB555 — alpha bit at position 5 always set to 1."""
    r = (r8 >> 3) & 0x1F
    g = (g8 >> 3) & 0x1F
    b = (b8 >> 3) & 0x1F
    return (b << 11) | (g << 6) | (1 << 5) | r

def make_frame(rows):
    """rows: 16 lists of 16 int (rgb555 values, 0=transparent)."""
    assert len(rows) == 16, f"expected 16 rows, got {len(rows)}"
    data = bytearray()
    for row in rows:
        assert len(row) == 16, f"expected 16 cols, got {len(row)}"
        for pixel in row:
            data.extend(struct.pack('<H', pixel))
    return bytes(data)

# ---------------------------------------------------------------------------
# Shared constants
# ---------------------------------------------------------------------------
T  = 0                           # transparent

# Crawler — PCB bug, top-down view
CB = rgb( 18,  55,  18)          # dark green body border
CG = rgb( 50, 130,  50)          # mid green carapace
CH = rgb(110, 230, 110)          # bright green highlight / carapace centre
GL = rgb(200, 165,  30)          # gold trace leg (leg position A)
GD = rgb(140, 115,  20)          # gold leg dim (leg position B)
LE = rgb(  0, 255,   0)          # green LED eye

# Flyer — glitchy data packet, blue/cyan
FB = rgb( 15,  15,  70)          # dark blue body border
FM = rgb( 40,  70, 200)          # mid blue
FH = rgb(130, 180, 255)          # bright blue highlight
FC = rgb(  0, 220, 220)          # cyan glitch pixel
FP = rgb(200,   0, 200)          # magenta glitch pixel
FW = rgb(255, 255, 255)          # white core

# Turret — capacitor body + mechanical base
TK = rgb( 30,  30,  30)          # near-black base / shadow
TG = rgb( 80,  80,  85)          # dark gray base
TS = rgb(160, 165, 170)          # silver highlight
TB = rgb( 20,  20, 110)          # dark blue capacitor body
TM = rgb( 55,  65, 200)          # mid blue cap
TH = rgb(130, 150, 255)          # bright blue cap highlight
TR = rgb(230,  20,   0)          # red LED idle
TY = rgb(255, 220,  30)          # yellow spark (armed)
TW = rgb(255, 255, 210)          # warm white spark centre

# =============================================================================
# Frame 0 — Crawler Walk A
# Arthropod viewed from above; upper legs extended, lower legs tucked.
# =============================================================================
frame0 = make_frame([
    # col: 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  0
    [  T,  T, GL,  T,  T,  T,  T,  T,  T,  T,  T, GL,  T,  T,  T,  T],  #  1 antennae tips
    [  T,  T,  T, GL,  T,  T,  T,  T,  T,  T, GL,  T,  T,  T,  T,  T],  #  2 antennae mid
    [  T,  T,  T, CB, CB, CB, CB, CB, CB, CB,  T,  T,  T,  T,  T,  T],  #  3 head
    [  T, GL, GL, CB, CG, CG, CG, CG, CG, CB, GL, GL,  T,  T,  T,  T],  #  4 upper legs A
    [  T, GL, CB, CG, CG, CH, CH, CH, CG, CG, CB, GL,  T,  T,  T,  T],  #  5 body top
    [  T,  T, CB, CG, CH, LE,  T, LE, CH, CG, CB,  T,  T,  T,  T,  T],  #  6 eye row
    [  T,  T, CB, CG, CG, CH, CH, CH, CG, CG, CB,  T,  T,  T,  T,  T],  #  7 body mid
    [  T, GD, CB, CG, CG, CG, CG, CG, CG, CG, CB, GD,  T,  T,  T,  T],  #  8 body bottom
    [  T, GD, GD, CB, CG, CG, CG, CG, CG, CB, GD, GD,  T,  T,  T,  T],  #  9 lower legs B
    [  T,  T,  T, CB, CB, CB, CB, CB, CB, CB,  T,  T,  T,  T,  T,  T],  # 10 tail
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 11
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 12
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 13
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 14
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 15
])

# =============================================================================
# Frame 1 — Crawler Walk B
# Upper legs tucked, lower legs extended (alternate stride).
# =============================================================================
frame1 = make_frame([
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  0
    [  T,  T, GD,  T,  T,  T,  T,  T,  T,  T,  T, GD,  T,  T,  T,  T],  #  1 antennae
    [  T,  T,  T, GD,  T,  T,  T,  T,  T,  T, GD,  T,  T,  T,  T,  T],  #  2
    [  T,  T,  T, CB, CB, CB, CB, CB, CB, CB,  T,  T,  T,  T,  T,  T],  #  3 head
    [  T, GD, GD, CB, CG, CG, CG, CG, CG, CB, GD, GD,  T,  T,  T,  T],  #  4 upper legs B
    [  T, GD, CB, CG, CG, CH, CH, CH, CG, CG, CB, GD,  T,  T,  T,  T],  #  5 body top
    [  T,  T, CB, CG, CH, LE,  T, LE, CH, CG, CB,  T,  T,  T,  T,  T],  #  6 eye row
    [  T,  T, CB, CG, CG, CH, CH, CH, CG, CG, CB,  T,  T,  T,  T,  T],  #  7 body mid
    [  T, GL, CB, CG, CG, CG, CG, CG, CG, CG, CB, GL,  T,  T,  T,  T],  #  8 body bottom
    [  T, GL, GL, CB, CG, CG, CG, CG, CG, CB, GL, GL,  T,  T,  T,  T],  #  9 lower legs A
    [  T,  T,  T, CB, CB, CB, CB, CB, CB, CB,  T,  T,  T,  T,  T,  T],  # 10 tail
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 11-15
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
])

# =============================================================================
# Frame 2 — Flyer Bob A (position nominal)
# Corrupt data packet — glitchy diamond with cyan/magenta artifacts.
# =============================================================================
frame2 = make_frame([
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  0
    [  T,  T,  T,  T,  T,  T, FP,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  1 glitch
    [  T,  T,  T,  T,  T, FB, FM, FB,  T,  T, FC,  T,  T,  T,  T,  T],  #  2
    [  T,  T,  T, FP, FB, FM, FH, FM, FB,  T,  T,  T,  T,  T,  T,  T],  #  3
    [  T,  T, FB, FM, FH, FW, FH, FW, FH, FM, FB,  T,  T,  T,  T,  T],  #  4 body wide
    [  T, FP, FM, FH, FW, FH, FH, FH, FW, FH, FM, FC,  T,  T,  T,  T],  #  5 widest row
    [  T,  T, FB, FM, FH, FW, FH, FW, FH, FM, FB,  T,  T,  T,  T,  T],  #  6
    [  T,  T,  T, FB, FM, FM, FH, FM, FM, FB,  T,  T,  T,  T,  T,  T],  #  7
    [  T, FC,  T,  T, FB, FM, FM, FM, FB,  T,  T, FP,  T,  T,  T,  T],  #  8 glitch
    [  T,  T,  T,  T,  T, FB, FB, FB,  T,  T,  T,  T,  T,  T,  T,  T],  #  9
    [  T,  T,  T,  T,  T,  T, FB,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 10
    [  T,  T, FP,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 11 glitch
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 12-15
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
])

# =============================================================================
# Frame 3 — Flyer Bob B (shifted 2px down — bob effect)
# =============================================================================
frame3 = make_frame([
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  0
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  1
    [  T,  T,  T,  T,  T,  T, FP,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  2 glitch
    [  T,  T,  T,  T,  T, FB, FM, FB,  T,  T, FC,  T,  T,  T,  T,  T],  #  3
    [  T,  T,  T, FP, FB, FM, FH, FM, FB,  T,  T,  T,  T,  T,  T,  T],  #  4
    [  T,  T, FB, FM, FH, FW, FH, FW, FH, FM, FB,  T,  T,  T,  T,  T],  #  5 body wide
    [  T, FP, FM, FH, FW, FH, FH, FH, FW, FH, FM, FC,  T,  T,  T,  T],  #  6 widest row
    [  T,  T, FB, FM, FH, FW, FH, FW, FH, FM, FB,  T,  T,  T,  T,  T],  #  7
    [  T,  T,  T, FB, FM, FM, FH, FM, FM, FB,  T,  T,  T,  T,  T,  T],  #  8
    [  T, FC,  T,  T, FB, FM, FM, FM, FB,  T,  T, FP,  T,  T,  T,  T],  #  9 glitch
    [  T,  T,  T,  T,  T, FB, FB, FB,  T,  T,  T,  T,  T,  T,  T,  T],  # 10
    [  T,  T,  T,  T,  T,  T, FB,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 11
    [  T,  T, FP,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 12 glitch
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 13-15
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
])

# =============================================================================
# Frame 4 — Turret Idle
# Cylindrical blue capacitor body on dark mechanical base; dim red LED.
# =============================================================================
frame4 = make_frame([
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  0
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  1
    [  T,  T,  T,  T, TB, TB, TB, TB, TB, TB,  T,  T,  T,  T,  T,  T],  #  2 cap top
    [  T,  T,  T, TB, TM, TH, TH, TH, TM, TM, TB,  T,  T,  T,  T,  T],  #  3 cap body
    [  T,  T,  T, TB, TM, TR,  T,  T, TM, TM, TB,  T,  T,  T,  T,  T],  #  4 LED dim
    [  T,  T,  T, TB, TM, TM, TM, TM, TM, TM, TB,  T,  T,  T,  T,  T],  #  5 cap body
    [  T,  T,  T, TB, TH, TM, TM, TM, TH, TM, TB,  T,  T,  T,  T,  T],  #  6 cap highlight
    [  T,  T,  T, TB, TB, TB, TB, TB, TB, TB, TB,  T,  T,  T,  T,  T],  #  7 cap bottom
    [  T, TK, TK, TG, TG, TG, TG, TG, TG, TG, TG, TK,  T,  T,  T,  T],  #  8 base top
    [  T, TG, TS, TS, TG, TG, TG, TG, TG, TS, TS, TG,  T,  T,  T,  T],  #  9 base mid
    [  T, TG, TG, TG, TG, TG, TG, TG, TG, TG, TG, TG,  T,  T,  T,  T],  # 10 base bottom
    [  T, TK, TK, TK, TK, TK, TK, TK, TK, TK, TK, TK,  T,  T,  T,  T],  # 11 shadow
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 12-15
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
])

# =============================================================================
# Frame 5 — Turret Armed
# Same body but bright red LED + yellow/white spark above — player nearby.
# =============================================================================
frame5 = make_frame([
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  #  0
    [  T,  T,  T,  T,  T, TY, TW, TY,  T,  T,  T,  T,  T,  T,  T,  T],  #  1 spark
    [  T,  T,  T,  T, TB, TW, TY, TW, TB, TB,  T,  T,  T,  T,  T,  T],  #  2 cap top + spark
    [  T,  T,  T, TB, TM, TH, TH, TH, TM, TM, TB,  T,  T,  T,  T,  T],  #  3 cap body
    [  T,  T,  T, TB, TM, TR, TW, TR, TM, TM, TB,  T,  T,  T,  T,  T],  #  4 LED BRIGHT
    [  T,  T,  T, TB, TM, TM, TM, TM, TM, TM, TB,  T,  T,  T,  T,  T],  #  5 cap body
    [  T,  T,  T, TB, TH, TM, TM, TM, TH, TM, TB,  T,  T,  T,  T,  T],  #  6 cap highlight
    [  T,  T,  T, TB, TB, TB, TB, TB, TB, TB, TB,  T,  T,  T,  T,  T],  #  7 cap bottom
    [  T, TK, TK, TG, TG, TG, TG, TG, TG, TG, TG, TK,  T,  T,  T,  T],  #  8 base top
    [  T, TG, TS, TS, TG, TG, TG, TG, TG, TS, TS, TG,  T,  T,  T,  T],  #  9 base mid
    [  T, TG, TG, TG, TG, TG, TG, TG, TG, TG, TG, TG,  T,  T,  T,  T],  # 10 base bottom
    [  T, TK, TK, TK, TK, TK, TK, TK, TK, TK, TK, TK,  T,  T,  T,  T],  # 11 shadow
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],  # 12-15
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
    [  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T,  T],
])

# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
frames = [frame0, frame1, frame2, frame3, frame4, frame5]

out_dir = os.path.dirname(__file__) or '.'
out_root = os.path.join(out_dir, '..', 'images')
os.makedirs(out_root, exist_ok=True)

data = bytearray()
for i, f in enumerate(frames):
    assert len(f) == FRAME_SIZE, f"frame {i} is {len(f)} bytes"
    data.extend(f)

out_path = os.path.join(out_root, 'EnemySprites.bin')
with open(out_path, 'wb') as f:
    f.write(data)

total = len(data)
print(f"EnemySprites.bin: {total} bytes ({len(frames)} frames × {FRAME_SIZE} bytes)")
