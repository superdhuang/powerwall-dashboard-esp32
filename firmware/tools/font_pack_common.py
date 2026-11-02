#!/usr/bin/env python3
"""Shared helpers: parse PWFONT1 packs and rasterize glyphs like the firmware generators."""
import struct
import freetype

HDR = "<8sIIII"; REC = "<40sHhhHiIIII"; GLY = "<IIIHHHhhBB"; KRN = "<IhH"


def parse_pack(path):
    d = open(path, "rb").read()
    magic, ver, nbytes, count, ck = struct.unpack(HDR, d[:24])
    recs = []
    for i in range(count):
        r = struct.unpack(REC, d[24 + 68 * i:24 + 68 * (i + 1)])
        name = r[0].split(b"\0")[0].decode()
        glyphs = {}
        for j in range(r[7]):
            g = struct.unpack(GLY, d[r[6] + 24 * j:r[6] + 24 * (j + 1)])
            cp, off, nb, adv, w, h, x, y, bpp, _ = g
            glyphs[cp] = dict(adv=adv, w=w, h=h, x=x, y=y, bitmap=d[off:off + nb])
        kern = {}
        for j in range(r[9]):
            pair, adj, _ = struct.unpack(KRN, d[r[8] + 8 * j:r[8] + 8 * (j + 1)])
            kern[pair] = adj
        recs.append(dict(name=name, height=r[1], baseline=r[2], underline=r[3], thickness=r[4],
                         fallback=r[5], glyphs=glyphs, kern=kern))
    return recs


def rasterize(face, cp, flags=freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL):
    face.load_char(cp, flags)
    g = face.glyph; b = g.bitmap
    px = []
    for y in range(b.rows):
        row = y * b.pitch if b.pitch >= 0 else (b.rows - 1 - y) * -b.pitch
        px.extend((b.buffer[row + x] * 15 + 127) // 255 for x in range(b.width))
    packed = bytes((px[i] << 4) | (px[i + 1] if i + 1 < len(px) else 0) for i in range(0, len(px), 2))
    return dict(adv=round(g.advance.x / 64), w=b.width, h=b.rows, x=g.bitmap_left,
                y=g.bitmap_top - b.rows, bitmap=packed)
