#!/usr/bin/env python3
"""Genera App/CirmoloSampler/icon.png (70x70) nello stile delle icone di spruce: tratto color oro
(215, 180, 95) su sfondo trasparente. Una griglia di pad (2x4, due banchi) con una forma d'onda sopra.
Disegno in 8x e riduzione.

Uso: python -I App/CirmoloSampler/src/tools/icona.py
"""
import math
import os

from PIL import Image, ImageDraw

QUI = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(QUI, '..', '..', 'icon.png')
S = 8
ORO = (215, 180, 95, 255)
LW = int(2.4 * S)


def main():
    im = Image.new('RGBA', (70 * S, 70 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # forma d'onda: un campione percussivo che decade, in alto
    x0, x1, cy = 8 * S, 62 * S, 20 * S
    pts = []
    n = 120
    for i in range(n + 1):
        t = i / n
        x = x0 + (x1 - x0) * t
        env = math.exp(-3.2 * t)
        y = cy - 11 * S * env * math.sin(2 * math.pi * 7.5 * t)
        pts.append((x, y))
    d.line(pts, fill=ORO, width=int(2.2 * S), joint='curve')
    # otto pad in due file: il banco sotto pieno, quello sopra vuoto
    px0, py0, pw, ph, gap = 8 * S, 36 * S, 11.2 * S, 10.5 * S, 3 * S
    for r in range(2):
        for c in range(4):
            x = px0 + c * (pw + gap)
            y = py0 + r * (ph + gap)
            box = [x, y, x + pw, y + ph]
            if r == 1 and c in (0, 2):
                d.rounded_rectangle(box, radius=2.2 * S, fill=ORO)
            else:
                d.rounded_rectangle(box, radius=2.2 * S, outline=ORO, width=LW)
    im.resize((70, 70), Image.LANCZOS).save(OUT, optimize=True)
    print('scritto', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
