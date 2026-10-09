#!/usr/bin/env python3
"""Genera App/CosaGioco/cosagioco.png (70x70) nello stile delle icone di spruce: un dado al tratto color oro
(215, 180, 95) con un punto di domanda al posto dei puntini. Disegno in 8x e riduzione; il tema Cirmolo
lo ricolora in viola (.github/branding/tema_cirmolo.py).
Uso: python -I App/CosaGioco/tools/icona.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

QUI = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(QUI, '..', 'cosagioco.png')
FONT = os.path.join(QUI, '..', '..', 'PyUI', 'fonts', 'BeVietnamPro-SemiBold.ttf')
S = 8
ORO = (215, 180, 95, 255)
LW = int(3.0 * S)


def main():
    im = Image.new('RGBA', (70 * S, 70 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([12 * S, 12 * S, 58 * S, 58 * S], radius=10 * S, outline=ORO, width=LW)
    # due puntini del dado agli angoli, il punto di domanda al centro
    for cx, cy in ((21, 21), (49, 49)):
        r = 3.4 * S
        d.ellipse([cx * S - r, cy * S - r, cx * S + r, cy * S + r], fill=ORO)
    font = ImageFont.truetype(FONT, 30 * S)
    d.text((35 * S, 36 * S), '?', font=font, fill=ORO, anchor='mm')
    im.resize((70, 70), Image.LANCZOS).save(OUT, optimize=True)
    print('scritto', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
