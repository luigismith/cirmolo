#!/usr/bin/env python3
"""Genera App/OpenOrc/icon.png (70x70) nello stile delle icone di spruce: un'orchidea al tratto color oro
(215, 180, 95): sepalo in alto, due petali larghi, due sepali in basso e il labello al centro.
Disegno in 8x e riduzione.
Uso: python -I App/OpenOrc/src/tools/icona.py
"""
import math
import os

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'icon.png')
S = 8
ORO = (215, 180, 95, 255)
LW = int(2.6 * S)


def petalo(d, cx, cy, rx, ry, gradi, punta=0.0):
    """Ellisse ruotata (con punta > 0 si assottiglia da un lato), disegnata come spezzata chiusa."""
    a = math.radians(gradi)
    punti = []
    for i in range(97):
        t = 2 * math.pi * i / 96
        x, y = rx * math.cos(t), ry * math.sin(t)
        if punta and y > 0:
            x *= 1.0 - punta * (y / ry) ** 2
        punti.append((cx + x * math.cos(a) - y * math.sin(a), cy + x * math.sin(a) + y * math.cos(a)))
    d.line(punti, fill=ORO, width=LW, joint='curve')


def main():
    im = Image.new('RGBA', (70 * S, 70 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx = 35 * S
    petalo(d, cx, 14 * S, 5.0 * S, 10.0 * S, 180, 0.55)                      # sepalo in alto
    for lato in (-1, 1):
        petalo(d, cx + lato * 15.5 * S, 27 * S, 7.5 * S, 12.0 * S, lato * 62, 0.35)   # petali larghi
        petalo(d, cx + lato * 12.5 * S, 49 * S, 4.2 * S, 10.0 * S, lato * 32, 0.6)   # sepali in basso
    # labello: una coppa con il bordo ondulato, e la colonna sopra
    d.ellipse([cx - 8.5 * S, 33 * S, cx + 8.5 * S, 52 * S], outline=ORO, width=LW)
    d.arc([cx - 4.5 * S, 36 * S, cx + 4.5 * S, 45 * S], 200, 340, fill=ORO, width=int(2.0 * S))
    d.ellipse([cx - 3.2 * S, 26.5 * S, cx + 3.2 * S, 32.5 * S], fill=ORO)
    im.resize((70, 70), Image.LANCZOS).save(OUT, optimize=True)
    print('scritto', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
