#!/usr/bin/env python3
"""Genera App/Fioritura/icon.png (70x70) nello stile delle icone di spruce: tratto color oro
(215, 180, 95) su sfondo trasparente. Un fiore a cinque petali con l'onda che si allarga attorno e
due semi accesi sulle corde. Disegno in 8x e riduzione.

Uso: python -I App/Fioritura/src/tools/icona.py
"""
import math
import os

from PIL import Image, ImageDraw

QUI = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(QUI, '..', '..', 'icon.png')
S = 8
ORO = (215, 180, 95, 255)


def main():
    im = Image.new('RGBA', (70 * S, 70 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy = 35 * S, 33 * S
    # l'onda: un cerchio sottile, aperto in basso a destra
    r = 27 * S
    d.arc([cx - r, cy - r, cx + r, cy + r], start=-200, end=70, fill=ORO, width=int(2.2 * S))
    # cinque petali a goccia
    for k in range(5):
        a = math.radians(-90 + 72 * k)
        pts = []
        for i in range(41):
            t = i / 40 * 2 * math.pi
            # goccia in coordinate locali: punta al centro, pancia fuori
            lx = (1 - math.cos(t)) * 0.5 * 15.5 * S
            ly = math.sin(t) * (1 - math.cos(t)) * 0.5 * 6.2 * S
            pts.append((cx + lx * math.cos(a) - ly * math.sin(a), cy + lx * math.sin(a) + ly * math.cos(a)))
        d.line(pts + [pts[0]], fill=ORO, width=int(2.2 * S), joint='curve')
    # il cuore
    rc = 3.4 * S
    d.ellipse([cx - rc, cy - rc, cx + rc, cy + rc], fill=ORO)
    # due semi sulle corde, fuori dall'onda
    for (x, y, rr) in ((61, 60, 3.0), (9, 58, 2.2)):
        d.ellipse([x * S - rr * S, y * S - rr * S, x * S + rr * S, y * S + rr * S], fill=ORO)
    im.resize((70, 70), Image.LANCZOS).save(OUT, optimize=True)
    print('scritto', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
