#!/usr/bin/env python3
"""Icone della voce Musica nella Home di PyUI (ic-music-n.png e ic-music-f.png), nello stile delle altre
icone della Home del tema SPRUCE: 100x140 trasparente, disegno nei 100x100 in alto, tratto di 6 px.
Come l'icona App (un cerchio con dentro tre righe) e' un cerchio, con dentro due crome unite.
Scrive le versioni SPRUCE (grigio caldo, oro quando e' scelta) e le stesse ricolorate per il tema
Cirmolo con la funzione di tema_cirmolo.py.

Uso: python -I .github/branding/icona_musica_home.py
"""
import math
import os
import sys

from PIL import Image, ImageDraw

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.abspath(os.path.join(QUI, '..', '..'))
sys.path.insert(0, QUI)
import tema_cirmolo as tc  # noqa: E402

S = 8
SPENTO = (124, 111, 100, 255)
ACCESO = (215, 180, 95, 255)


def ellisse_ruotata(cx, cy, rx, ry, ang):
    pts = []
    for i in range(48):
        t = 2 * math.pi * i / 48
        x, y = rx * math.cos(t), ry * math.sin(t)
        pts.append(((cx + x * math.cos(ang) - y * math.sin(ang)) * S, (cy + x * math.sin(ang) + y * math.cos(ang)) * S))
    return pts


def disegna(colore):
    im = Image.new('RGBA', (100 * S, 140 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # il cerchio, come nell'icona App
    r, w = 42.0, 6.0
    d.ellipse([(50 - r) * S, (50 - r) * S, (50 + r) * S, (50 + r) * S], outline=colore, width=int(w * S))
    # due crome unite: teste piene e inclinate, gambi, una trave
    ang = math.radians(-24)
    for cx, cy in ((37.5, 64.0), (62.5, 57.5)):
        d.polygon(ellisse_ruotata(cx, cy, 8.6, 6.4, ang), fill=colore)
    gambo = 5.0
    for x, y0, y1 in ((44.6, 62.0, 30.5), (69.6, 55.5, 24.0)):
        d.rectangle([(x - gambo) * S, y1 * S, x * S, y0 * S], fill=colore)
    d.polygon([(39.6 * S, 31.5 * S), (69.6 * S, 24.0 * S), (69.6 * S, 32.5 * S), (39.6 * S, 40.0 * S)], fill=colore)
    return im.resize((100, 140), Image.LANCZOS)


def main():
    for nome, colore in (('ic-music-n.png', SPENTO), ('ic-music-f.png', ACCESO)):
        im = disegna(colore)
        spruce = os.path.join(RADICE, 'Themes', 'SPRUCE', 'skin', nome)
        im.save(spruce, optimize=True)
        cirmolo = os.path.join(RADICE, 'Themes', 'Cirmolo', 'skin', nome)
        tc.ricolora(im).save(cirmolo, optimize=True)
        print('scritte', os.path.relpath(spruce, RADICE), 'e', os.path.relpath(cirmolo, RADICE))


if __name__ == '__main__':
    main()
