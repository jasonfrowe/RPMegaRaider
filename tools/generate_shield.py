#!/usr/bin/env python3
"""generate_shield.py — Shield overlay sprite for RPMegaRaider.

Produces images/Shield.bin (3 frames × 32×32 × 2 bytes = 6144 bytes).

XRAM layout (base = SHIELD_SPRITE_BASE = 0x9000):
  Frame 0: base + 0x0000  (3+ charges — full bright cyan ring)
  Frame 1: base + 0x0800  (2  charges — dotted blue ring)
  Frame 2: base + 0x1000  (1  charge  — sparse orange arc segments)

Pixel format: little-endian RGB555 with alpha bit:
  bit[15:11] = B5   bit[10:6] = G5   bit[5] = alpha (1=opaque)   bit[4:0] = R5
  0x0000 = fully transparent
"""

import math
import os
import struct

FRAME_W = 32
FRAME_H = 32
FRAME_SIZE = FRAME_W * FRAME_H * 2   # 2048 bytes per frame


def rgb(r8, g8, b8):
    """Return an opaque RGB555 pixel (alpha bit set)."""
    r = (r8 >> 3) & 0x1F
    g = (g8 >> 3) & 0x1F
    b = (b8 >> 3) & 0x1F
    return (b << 11) | (g << 6) | (1 << 5) | r


T = 0  # transparent pixel

COLOR_FULL = rgb(  0, 224, 255)   # bright cyan   — 3-charge shield
COLOR_MED  = rgb(  0, 160, 220)   # medium blue   — 2-charge shield
COLOR_LOW  = rgb(255, 100,   0)   # warning orange — 1-charge shield

# Ring geometry — centred on the 32×32 sprite
CX = (FRAME_W - 1) / 2.0   # 15.5
CY = (FRAME_H - 1) / 2.0   # 15.5
RING_MIN = 9.0              # inner radius in pixels
RING_MAX = 12.0             # outer radius in pixels


def is_ring(x, y):
    dx = x - CX
    dy = y - CY
    r = math.sqrt(dx * dx + dy * dy)
    return RING_MIN <= r <= RING_MAX


def make_frame(color, density):
    """
    Build one 32×32 frame.

    density:
      'full'   — every ring pixel is opaque (solid circle)
      'half'   — checkerboard pattern on the ring (dotted look)
      'sparse' — 8 arc segments separated by gaps (minimal coverage)
    """
    data = bytearray()
    for y in range(FRAME_H):
        for x in range(FRAME_W):
            if not is_ring(x, y):
                data.extend(struct.pack('<H', T))
                continue

            if density == 'full':
                data.extend(struct.pack('<H', color))

            elif density == 'half':
                # Alternate pixel on / off using sum of coords
                pix = color if (x + y) % 2 == 0 else T
                data.extend(struct.pack('<H', pix))

            else:  # 'sparse' — 8 short arc segments, ~20° wide, every 45°
                dx = x - CX
                dy = y - CY
                angle_deg = (math.degrees(math.atan2(dy, dx)) + 360.0) % 360.0
                pix = color if angle_deg % 45.0 < 20.0 else T
                data.extend(struct.pack('<H', pix))

    assert len(data) == FRAME_SIZE, f"Frame size mismatch: {len(data)}"
    return bytes(data)


frame0 = make_frame(COLOR_FULL, 'full')    # 3+ charges
frame1 = make_frame(COLOR_MED,  'half')    # 2 charges
frame2 = make_frame(COLOR_LOW,  'sparse')  # 1 charge

out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'images')
os.makedirs(out_dir, exist_ok=True)
out_path = os.path.join(out_dir, 'Shield.bin')

with open(out_path, 'wb') as f:
    for frame in [frame0, frame1, frame2]:
        f.write(frame)

total = FRAME_SIZE * 3
print(f"Shield.bin: {total} bytes — 3 frames × {FRAME_SIZE} bytes (32×32 RGB555)")
print(f"  Frame 0 @ XRAM +0x0000: solid cyan ring   (3+ charges)")
print(f"  Frame 1 @ XRAM +0x0800: dotted blue ring  (2 charges)")
print(f"  Frame 2 @ XRAM +0x1000: sparse orange arcs (1 charge)")
print(f"Written → {out_path}")
