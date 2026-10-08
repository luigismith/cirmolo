#!/usr/bin/env python3
"""Sostituisce l'albero di spruce con la pigna di Cirmolo nelle immagini di sistema (spruce/imgs):
messaggi di accensione, spegnimento e riavvio, riparazione della SD, Syncthing.

Stesse dimensioni e stesso stile piatto degli originali:
- tree_sm_close_crop.png  88x152, sagoma bianca su trasparente (icona dei messaggi e dello spegnimento)
- bg_tree_sm.png          640x480, sagoma bianca piccola al centro su grigio scuro
- bg_tree.png             640x480, sagoma grande appena visibile (sfondo dei messaggi)
- bg_tree_wide.png, bg_tree_sm_wide.png  854x480, le stesse per gli schermi 16:9

Uso: python -I .github/branding/immagini_sistema.py
"""
import os

from PIL import Image, ImageChops, ImageDraw

RADICE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
IMGS = os.path.join(RADICE, 'spruce', 'imgs')
SS = 8
FONDO = (40, 40, 40)


def pigna(w, h, colore):
    """La pigna del logo di avvio (logo_avvio.py), resa in toni di grigio e usata come trasparenza:
    una sagoma del colore richiesto, con le squame in rilievo."""
    import sys
    import numpy as np
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import logo_avvio as la
    la.VIOLA_SCURO = np.array([70, 70, 70], np.float32)
    la.VIOLA = np.array([175, 175, 175], np.float32)
    la.VIOLA_CHIARO = np.array([255, 255, 255], np.float32)
    la.SOLCO = np.array([0, 0, 0], np.float32)
    la.DY = 0
    pad = 12                                  # margine: il disegno della pigna sborda di qualche pixel
    img = np.zeros(((h + 2 * pad) * la.SS, (w + 2 * pad) * la.SS, 3), np.float32)
    rx = w / 2.0 / 1.12 - 1
    ry = h / 2.0 - 2
    la.pigna(img, w / 2.0 + pad, h / 2.0 + pad, rx, ry)
    img = img[pad * la.SS:(pad + h) * la.SS, pad * la.SS:(pad + w) * la.SS]
    lum = Image.fromarray(np.clip(img.mean(axis=2), 0, 255).astype(np.uint8), 'L').resize((w, h), Image.LANCZOS)
    out = Image.new('RGBA', (w, h), colore + (0,))
    out.putalpha(lum)
    return out


def sfondo(w, h, icona_w, icona_h, colore):
    im = Image.new('RGBA', (w, h), FONDO + (255,))
    p = pigna(icona_w, icona_h, colore)
    im.alpha_composite(p, ((w - icona_w) // 2, (h - icona_h) // 2 - 8))
    return im


def main():
    os.makedirs(IMGS, exist_ok=True)
    pigna(88, 128, (255, 255, 255)).save(os.path.join(IMGS, 'tree_sm_close_crop.png'), optimize=True)
    sfondo(640, 480, 96, 140, (255, 255, 255)).save(os.path.join(IMGS, 'bg_tree_sm.png'), optimize=True)
    sfondo(640, 480, 176, 304, (51, 51, 51)).save(os.path.join(IMGS, 'bg_tree.png'), optimize=True)
    sfondo(854, 480, 96, 140, (255, 255, 255)).convert('RGB').save(os.path.join(IMGS, 'bg_tree_sm_wide.png'), optimize=True)
    sfondo(854, 480, 176, 304, (51, 51, 51)).convert('RGB').save(os.path.join(IMGS, 'bg_tree_wide.png'), optimize=True)
    print('scritte le immagini in', IMGS)


if __name__ == '__main__':
    main()
