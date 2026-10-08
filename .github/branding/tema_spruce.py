#!/usr/bin/env python3
"""Nel tema SPRUCE (Themes/SPRUCE/skin*/bg-title.png) la barra del titolo di ogni menu ha in alto a sinistra
l'albero di spruce. Lo sostituisce con la pigna di Cirmolo: stessa posizione, stessa altezza, stesso colore.

La pigna e' quella del logo di avvio (logo_avvio.pigna), resa in toni di grigio: la copertura fa da
trasparenza, il chiaroscuro modula il colore dell'albero originale.
Si puo' rilanciare: se l'albero non c'e' piu' (pigna gia' messa) il file resta com'e'.

Uso: python -I .github/branding/tema_spruce.py
"""
import glob
import os
import sys

import numpy as np
from PIL import Image

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.abspath(os.path.join(QUI, '..', '..'))
sys.path.insert(0, QUI)
import logo_avvio as la  # noqa: E402

MARCA = os.path.join(QUI, 'tema_spruce.fatto')


def render(w, h, palette):
    la.VIOLA_SCURO, la.VIOLA, la.VIOLA_CHIARO, la.SOLCO = [np.array(c, np.float32) for c in palette]
    la.DY = 0
    pad = 12
    img = np.zeros(((h + 2 * pad) * la.SS, (w + 2 * pad) * la.SS, 3), np.float32)
    la.pigna(img, w / 2.0 + pad, h / 2.0 + pad, w / 2.0 / 1.12 - 0.5, h / 2.0 - 0.5)
    img = img[pad * la.SS:(pad + h) * la.SS, pad * la.SS:(pad + w) * la.SS]
    return np.asarray(Image.fromarray(np.clip(img.mean(axis=2), 0, 255).astype(np.uint8), 'L').resize((w, h), Image.LANCZOS), np.float32) / 255.0


def main():
    for path in sorted(glob.glob(os.path.join(RADICE, 'Themes', 'SPRUCE', 'skin*', 'bg-title.png'))):
        im = Image.open(path).convert('RGBA')
        a = np.asarray(im, np.float32).copy()
        fondo = a[a.shape[0] // 2, -5, :3]
        zona = a[:, :200, :3]
        diff = np.abs(zona - fondo).sum(axis=2) > 30
        ys, xs = np.nonzero(diff)
        if len(xs) == 0:
            print('nessun albero, lascio stare:', os.path.relpath(path, RADICE))
            continue
        x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
        colore = zona[diff].mean(axis=0)
        alberello = (y1 - y0) / (x1 - x0)
        if alberello < 1.3:                       # la pigna e' meno slanciata: e' gia' stata sostituita
            print('gia\' fatto:', os.path.relpath(path, RADICE))
            continue
        a[y0:y1, x0:x1, :3] = fondo               # via l'albero
        h = y1 - y0
        w = int(round(h * 0.68))
        cx = (x0 + x1) // 2
        px0 = cx - w // 2
        copertura = render(w, h, [(255, 255, 255)] * 3 + [(255, 255, 255)])
        chiaroscuro = render(w, h, [(150, 150, 150), (215, 215, 215), (255, 255, 255), (60, 60, 60)])
        tono = np.clip(chiaroscuro / np.maximum(copertura, 1e-3), 0.0, 1.0)
        col = colore[None, None, :] * (0.55 + 0.65 * tono[..., None])
        al = copertura[..., None]
        reg = a[y0:y0 + h, px0:px0 + w, :3]
        a[y0:y0 + h, px0:px0 + w, :3] = reg * (1 - al) + np.clip(col, 0, 255) * al
        Image.fromarray(np.clip(np.round(a), 0, 255).astype(np.uint8), 'RGBA').save(path, optimize=True)
        print('pigna al posto dell\'albero:', os.path.relpath(path, RADICE), (int(x0), int(y0), int(x1), int(y1)))


if __name__ == '__main__':
    main()
