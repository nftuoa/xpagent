#!/usr/bin/env python3
"""Generate a Windows XP-style multi-size RGBA .ico file (16, 32, 48 px)."""

import math
import os
import struct


def make_bmp_data(size: int) -> bytes:
    # BITMAPINFOHEADER (40 bytes) + BGRA pixels (bottom-up) + 1-bit AND mask
    cx = size / 2.0
    cy = size / 2.0
    radius = size * 0.42
    pixels = bytearray()

    for y_top in range(size - 1, -1, -1):  # bottom-up for ICO BMP
        for x in range(size):
            dx = (x + 0.5) - cx
            dy = (y_top + 0.5) - cy
            dist = math.hypot(dx, dy)

            # Soft drop shadow offset (+1.5, +1.8)
            sdist = math.hypot((x + 0.5) - (cx + size * 0.05), (y_top + 0.5) - (cy + size * 0.06))

            if dist <= radius:
                # XP Luna glossy blue orb gradient
                nx = dx / radius
                ny = dy / radius
                # Gloss highlight at top-left
                gloss = max(0.0, 1.0 - math.hypot(nx + 0.25, ny + 0.35) * 1.15)
                base_r = int(28 + (1.0 - (ny + 1.0) * 0.5) * 45)
                base_g = int(95 + (1.0 - (ny + 1.0) * 0.5) * 85)
                base_b = int(205 + (1.0 - (ny + 1.0) * 0.5) * 50)

                # Rim darkening
                if dist > radius - max(1.2, size * 0.05):
                    base_r = int(base_r * 0.55)
                    base_g = int(base_g * 0.60)
                    base_b = int(base_b * 0.72)

                # Blend white gloss
                r = min(255, int(base_r + gloss * 170))
                g = min(255, int(base_g + gloss * 145))
                b = min(255, int(base_b + gloss * 75))
                a = 255

                # Crisp white "XP" / AI sparkle cross in center
                if abs(nx) < 0.09 and abs(ny) < 0.42:
                    r, g, b = 255, 255, 255
                elif abs(ny) < 0.09 and abs(nx) < 0.42:
                    r, g, b = 255, 255, 255
                elif abs(nx) + abs(ny) < 0.24:
                    r, g, b = 255, 245, 190

                # Anti-aliased outer edge
                edge = radius - dist
                if edge < 1.0:
                    a = max(0, min(255, int(edge * 255)))
                pixels.extend((b, g, r, a))
            elif sdist <= radius:
                alpha = max(0, min(90, int((radius - sdist) * 45)))
                pixels.extend((0, 0, 0, alpha))
            else:
                pixels.extend((0, 0, 0, 0))

    # AND mask: 1 bit per pixel, padded to 4-byte rows (all 0 since alpha channel handles transparency)
    row_bytes = ((size + 31) // 32) * 4
    and_mask = b"\x00" * (row_bytes * size)

    bih = struct.pack(
        "<IIIHHIIIIII",
        40,              # biSize
        size,            # biWidth
        size * 2,        # biHeight (XOR + AND)
        1,               # biPlanes
        32,              # biBitCount
        0,               # biCompression (BI_RGB)
        len(pixels) + len(and_mask),
        0, 0, 0, 0,
    )
    return bih + bytes(pixels) + and_mask


def write_ico(path: str) -> None:
    sizes = [16, 32, 48]
    images = [make_bmp_data(s) for s in sizes]
    header = struct.pack("<HHH", 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    entries = bytearray()
    for s, img in zip(sizes, images):
        entries.extend(struct.pack("<BBBBHHII", s, s, 0, 0, 1, 32, len(img), offset))
        offset += len(img)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(header)
        f.write(entries)
        for img in images:
            f.write(img)


if __name__ == "__main__":
    out_path = os.path.join(os.path.dirname(__file__), "..", "resources", "app.ico")
    write_ico(os.path.abspath(out_path))
    print("Generated", os.path.abspath(out_path))
