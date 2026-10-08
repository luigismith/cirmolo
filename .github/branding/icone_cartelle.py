#!/usr/bin/env python3
"""Icone delle aree tematiche del menu App (App/PyUI/folder-icons/*.png), 70x70, nello stile delle icone
di spruce: tratto color oro (215, 180, 95) su sfondo trasparente. Disegno in 8x e riduzione.

Uso: python -I .github/branding/icone_cartelle.py
"""
import math
import os

from PIL import Image, ImageDraw

RADICE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(RADICE, 'App', 'PyUI', 'folder-icons')
S = 8
ORO = (215, 180, 95, 255)
LW = int(3.0 * S)


def tela():
    im = Image.new('RGBA', (70 * S, 70 * S), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def p(x, y):
    return (x * S, y * S)


def salva(im, nome):
    im.resize((70, 70), Image.LANCZOS).save(os.path.join(OUT, nome), optimize=True)


def musica():
    im, d = tela()
    # due crome unite da una barra
    d.ellipse([*p(13, 44), *p(25, 54)], fill=ORO)
    d.ellipse([*p(39, 40), *p(51, 50)], fill=ORO)
    d.line([p(24, 49), p(24, 16)], fill=ORO, width=LW)
    d.line([p(50, 45), p(50, 12)], fill=ORO, width=LW)
    d.polygon([p(22.5, 14), p(51.5, 8), p(51.5, 16), p(22.5, 22)], fill=ORO)
    salva(im, 'musica.png')


def giochi():
    im, d = tela()
    d.rounded_rectangle([*p(8, 20), *p(62, 50)], radius=13 * S, outline=ORO, width=LW)
    d.line([p(22, 29), p(22, 41)], fill=ORO, width=LW)
    d.line([p(16, 35), p(28, 35)], fill=ORO, width=LW)
    for cx, cy in ((46, 31), (53, 38)):
        d.ellipse([*p(cx - 3, cy - 3), *p(cx + 3, cy + 3)], fill=ORO)
    salva(im, 'giochi.png')


def media():
    im, d = tela()
    d.rounded_rectangle([*p(9, 14), *p(61, 56)], radius=5 * S, outline=ORO, width=LW)
    d.line([p(13, 50), p(28, 32), p(38, 43), p(45, 36), p(57, 50)], fill=ORO, width=LW, joint='curve')
    d.ellipse([*p(44, 20), *p(52, 28)], fill=ORO)
    salva(im, 'media.png')


def personalizza():
    im, d = tela()
    # tavolozza: ovale con un incavo e tre macchie di colore
    d.ellipse([*p(9, 13), *p(61, 57)], outline=ORO, width=LW)
    d.ellipse([*p(38, 38), *p(50, 50)], fill=(0, 0, 0, 0), outline=ORO, width=LW)
    for cx, cy in ((22, 27), (34, 21), (47, 26), (21, 41)):
        d.ellipse([*p(cx - 4, cy - 4), *p(cx + 4, cy + 4)], fill=ORO)
    salva(im, 'personalizza.png')


def sistema():
    im, d = tela()
    cx, cy = 35, 35
    for i in range(8):                      # denti larghi attorno a un anello
        a = 2 * math.pi * i / 8
        d.line([p(cx + 15 * math.cos(a), cy + 15 * math.sin(a)), p(cx + 24 * math.cos(a), cy + 24 * math.sin(a))],
               fill=ORO, width=int(8 * S))
    d.ellipse([*p(cx - 17, cy - 17), *p(cx + 17, cy + 17)], fill=ORO)
    d.ellipse([*p(cx - 7, cy - 7), *p(cx + 7, cy + 7)], fill=(0, 0, 0, 0))
    salva(im, 'sistema.png')


def altro():
    im, d = tela()
    for x in (13, 38):
        for y in (13, 38):
            d.rounded_rectangle([*p(x, y), *p(x + 19, y + 19)], radius=5 * S, outline=ORO, width=LW)
    salva(im, 'altro.png')


def main():
    os.makedirs(OUT, exist_ok=True)
    for f in (musica, giochi, media, personalizza, sistema, altro):
        f()
    print('icone scritte in', OUT)


if __name__ == '__main__':
    main()
