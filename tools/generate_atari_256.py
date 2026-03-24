import numpy as np
from PIL import Image

def generate_atari_256():
    # 16 Hues x 16 Lumas = 256 colors
    width, height = 16, 16
    data = np.zeros((height, width, 3), dtype=np.uint8)

    # NTSC conversion matrix: YIQ to RGB
    # R = Y + 0.956*I + 0.621*Q
    # G = Y - 0.272*I - 0.647*Q
    # B = Y - 1.106*I + 1.703*Q
    
    for h in range(16):      # X axis = Hue
        for l in range(16):  # Y axis = Luma (0-15)
            
            y = l / 15.0  # Normalize Luma to 0.0 - 1.0
            
            if h == 0:
                # Hue 0 is grayscale in NTSC/Atari
                i, q = 0, 0
            else:
                # Hues 1-15 are phase shifts (24 degrees apart)
                # Phase offset of 25.7° aligns hue 1 with classic Atari gold/orange
                phase = (h - 1) * (2 * np.pi / 15) + np.radians(25.7)
                sat = 0.25  # Actual Atari saturation; 0.15 is too low and causes duplicate colors
                i = sat * np.cos(phase)
                q = sat * np.sin(phase)

            # Convert YIQ to RGB
            r = y + 0.956 * i + 0.621 * q
            g = y - 0.272 * i - 0.647 * q
            b = y - 1.106 * i + 1.703 * q

            # Clamp, round, and scale to 0-255
            # np.round() is required — truncation (the default for uint8 assignment)
            # causes adjacent hues at low luma to collapse to the same value.
            data[l, h] = [
                int(np.round(np.clip(r * 255, 0, 255))),
                int(np.round(np.clip(g * 255, 0, 255))),
                int(np.round(np.clip(b * 255, 0, 255)))
            ]

    img = Image.fromarray(data, 'RGB')
    img.save('atari_256_refined.png')

    unique = len({tuple(data[l, h]) for l in range(16) for h in range(16)})
    print(f"Atari NTSC 256-color palette generated. Unique colors: {unique}")

if __name__ == "__main__":
    generate_atari_256()