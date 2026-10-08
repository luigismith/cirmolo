#!/usr/bin/env python3
"""Genera App/Diapason/icon.png (70x70) nello stile delle icone di spruce: un diapason al tratto
color oro (215, 180, 95) con due piccole onde. Disegno in 8x e riduzione.
Uso: python -I App/Diapason/src/tools/icona.py
"""
import math
import os

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'icon.png')
S = 8
ORO = (215, 180, 95, 255)
LW = int(3.0 * S)


def main():
    im = Image.new('RGBA', (70 * S, 70 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # rebbi a U e manico
    cx, top, base = 35 * S, 9 * S, 40 * S
    half = 7 * S
    d.line([(cx - half, top), (cx - half, base - half)], fill=ORO, width=LW)
    d.line([(cx + half, top), (cx + half, base - half)], fill=ORO, width=LW)
    d.arc([cx - half, base - 2 * half, cx + half, base], 0, 180, fill=ORO, width=LW)
    d.line([(cx, base), (cx, 62 * S)], fill=ORO, width=LW)
    # onde ai lati
    for side in (-1, 1):
        for k, r in enumerate((6 * S, 11 * S)):
            x0 = cx + side * (half + 5 * S)
            box = [x0 - r, 22 * S - r, x0 + r, 22 * S + r]
            start, end = (-50, 50) if side > 0 else (130, 230)
            d.arc(box, start, end, fill=ORO, width=int(2.4 * S))
    im.resize((70, 70), Image.LANCZOS).save(OUT, optimize=True)
    print('scritto', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
