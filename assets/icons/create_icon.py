"""Rebuild the app's vector source, PNG preview and multi-resolution Windows icon (stdlib only)."""
from pathlib import Path
import struct
import zlib

OUT = Path(__file__).resolve().parent
# A grass-green cube with two server bays on its front face.
SHAPES = [
    ('#172b29', [(32, 2), (59, 17), (59, 47), (32, 62), (5, 47), (5, 17)]),
    ('#81db83', [(32, 7), (54, 19), (32, 31), (10, 19)]),
    ('#30875c', [(10, 23), (30, 34), (30, 55), (10, 44)]),
    ('#205347', [(34, 34), (54, 23), (54, 44), (34, 55)]),
    ('#b6f1ac', [(32, 11), (45, 18), (39, 21), (26, 14)]),
    ('#112f2a', [(37, 36), (51, 28), (51, 34), (37, 42)]),
    ('#112f2a', [(37, 46), (51, 38), (51, 44), (37, 52)]),
    ('#9af0a1', [(47, 32), (49, 31), (49, 33), (47, 34)]),
    ('#9af0a1', [(47, 42), (49, 41), (49, 43), (47, 44)]),
]
svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">\n'
for color, points in SHAPES:
    svg += f'  <polygon fill="{color}" points="' + ' '.join(f'{x},{y}' for x, y in points) + '"/>\n'
(OUT / 'app.svg').write_text(svg + '</svg>\n')

def inside(x, y, points):
    hit = False
    j = len(points) - 1
    for i, (xi, yi) in enumerate(points):
        xj, yj = points[j]
        if (yi > y) != (yj > y) and x < (xj-xi)*(y-yi)/(yj-yi)+xi:
            hit = not hit
        j = i
    return hit

def png(size):
    raw = bytearray()
    colors = [(tuple(bytes.fromhex(c[1:])), p) for c, p in SHAPES]
    samples = 4
    for y in range(size):
        raw.append(0)
        for x in range(size):
            sums = [0, 0, 0]
            count = 0
            for sy in range(samples):
                for sx in range(samples):
                    px = (x + (sx+.5)/samples)*64/size
                    py = (y + (sy+.5)/samples)*64/size
                    for color, points in reversed(colors):
                        if inside(px, py, points):
                            for k in range(3):
                                sums[k] += color[k]
                            count += 1
                            break
            raw.extend([*(round(v/count) if count else 0 for v in sums), round(count*255/(samples*samples))])
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind+data))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')

sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
images = [png(s) for s in sizes]
offset = 6 + 16*len(sizes)
entries = []
for size, data in zip(sizes, images):
    entries.append(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
    offset += len(data)
(OUT / 'app.ico').write_bytes(struct.pack('<HHH', 0, 1, len(sizes)) + b''.join(entries) + b''.join(images))
(OUT / 'app.png').write_bytes(images[-1])
print('Created app.svg, app.png, app.ico (9 sizes)')
