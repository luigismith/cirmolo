#!/usr/bin/env python3
"""Genera App/ClaudeChat/icon.png (70x70) nello stile delle icone di spruce: un fumetto al tratto color oro
(215, 180, 95) con tre puntini, come una risposta che sta arrivando. Disegno in 8x e riduzione.
Uso: python -I App/ClaudeChat/src/tools/icona.py
"""
import os

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'icon.png')
S = 8
ORO = (215, 180, 95, 255)
LW = int(3.0 * S)


def main():
    im = Image.new('RGBA', (70 * S, 70 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    box = [8 * S, 12 * S, 62 * S, 48 * S]
    d.rounded_rectangle(box, radius=12 * S, outline=ORO, width=LW)
    # coda del fumetto, in basso a sinistra
    d.polygon([(19 * S, 46 * S), (16 * S, 60 * S), (31 * S, 46 * S)], fill=ORO)
    d.polygon([(21.5 * S, 44 * S), (19.8 * S, 52 * S), (27.5 * S, 44 * S)], fill=(0, 0, 0, 0))
    for k in range(3):
        cx, cy, r = (23 + k * 12) * S, 30 * S, 3.6 * S
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=ORO)
    im.resize((70, 70), Image.LANCZOS).save(OUT, optimize=True)
    print('scritto', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
