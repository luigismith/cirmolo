#!/usr/bin/env python3
"""Genera App/CirmoloSynth/icon.png (70x70) nello stile delle icone di spruce: tratto color oro
(215, 180, 95) su sfondo trasparente. Una tastiera con un'onda sopra: a 70 pixel si riconosce
meglio di una pigna (che resta il logo dentro l'app). Disegno in 8x e riduzione.

Uso: python -I App/CirmoloSynth/src/tools/icona.py
"""
import math
import os

from PIL import Image, ImageDraw

QUI = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(QUI, '..', '..', 'icon.png')
S = 8
ORO = (215, 180, 95, 255)
LW = int(2.6 * S)


def main():
    im = Image.new('RGBA', (70 * S, 70 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # onda
    punti = [(14 * S + t / 100 * 42 * S, 19 * S - math.sin(t / 100 * math.pi * 2) * 6 * S) for t in range(101)]
    d.line(punti, fill=ORO, width=LW, joint='curve')
    # tastiera: 6 tasti bianchi, 4 neri
    x0, y0, x1, y1 = 11 * S, 32 * S, 59 * S, 58 * S
    d.rounded_rectangle([x0, y0, x1, y1], radius=4 * S, outline=ORO, width=LW)
    w = (x1 - x0) / 6
    for i in range(1, 6):
        x = x0 + i * w
        d.line([(x, y0 + 14 * S), (x, y1)], fill=ORO, width=int(2 * S))
    for i in (1, 2, 4, 5):
        x = x0 + i * w
        d.rounded_rectangle([x - 2.4 * S, y0, x + 2.4 * S, y0 + 15 * S], radius=1.2 * S, fill=ORO)
    im.resize((70, 70), Image.LANCZOS).save(OUT, optimize=True)
    print('scritto', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
