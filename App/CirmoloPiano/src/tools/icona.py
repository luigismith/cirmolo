#!/usr/bin/env python3
"""Genera App/CirmoloPiano/icon.png (70x70) nello stile delle icone di spruce: tratto color oro
(215, 180, 95) su sfondo trasparente. Una tastiera di pianoforte con una nota sopra (Cirmolo Synth ha
l'onda sopra i tasti). Disegno in 8x e riduzione.

Uso: python -I App/CirmoloPiano/src/tools/icona.py
"""
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
    # nota (semiminima): testa inclinata, gambo e bandierina
    cx, cy = 30 * S, 22 * S
    d.ellipse([cx - 6 * S, cy - 4.4 * S, cx + 6 * S, cy + 4.4 * S], fill=ORO)
    d.line([(cx + 5.2 * S, cy - 1 * S), (cx + 5.2 * S, cy - 17 * S)], fill=ORO, width=int(2.2 * S))
    d.line([(cx + 5.2 * S, cy - 17 * S), (cx + 13 * S, cy - 11 * S), (cx + 11 * S, cy - 4 * S)],
           fill=ORO, width=int(2.2 * S), joint='curve')
    # tastiera: 7 tasti bianchi, 5 neri
    x0, y0, x1, y1 = 8 * S, 33 * S, 62 * S, 60 * S
    d.rounded_rectangle([x0, y0, x1, y1], radius=4 * S, outline=ORO, width=LW)
    w = (x1 - x0) / 7
    for i in range(1, 7):
        x = x0 + i * w
        d.line([(x, y0 + 15 * S), (x, y1)], fill=ORO, width=int(2 * S))
    for i in (1, 2, 4, 5, 6):
        x = x0 + i * w
        d.rounded_rectangle([x - 2.3 * S, y0, x + 2.3 * S, y0 + 16 * S], radius=1.2 * S, fill=ORO)
    im.resize((70, 70), Image.LANCZOS).save(OUT, optimize=True)
    print('scritto', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
