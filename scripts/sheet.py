#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""The simulator's PGM shots as one PNG contact sheet: sheet.py SHOT_DIR OUT.png [COLUMNS]"""
import glob, os, struct, sys, zlib

def pgm(path):
    d = open(path, 'rb').read()
    parts = d.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]

def main():
    files = sorted(glob.glob(os.path.join(sys.argv[1], '*.pgm')))
    cols = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    w, h, _ = pgm(files[0])
    gap = 12
    rows = (len(files) + cols - 1) // cols
    W, H = cols * (w + gap) + gap, rows * (h + gap) + gap
    img = [bytearray([40] * W) for _ in range(H)]
    for i, f in enumerate(files):
        _, _, px = pgm(f)
        x0, y0 = gap + (i % cols) * (w + gap), gap + (i // cols) * (h + gap)
        for y in range(h):
            img[y0 + y][x0:x0 + w] = px[y * w:(y + 1) * w]
    raw = b''.join(b'\0' + bytes(r) for r in img)
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(sys.argv[2], 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 0, 0, 0, 0))
                                  + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
    print(sys.argv[2], len(files), 'shots')

main()
