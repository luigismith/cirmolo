#!/usr/bin/env python3
"""Immagini di sistema di Cirmolo (spruce/imgs): le schermate che gli script mostrano fuori dal menu
(accensione, spegnimento e riavvio, riparazione della SD, backup, aggiornamenti, Syncthing...).

- Dove spruce aveva il suo albero c'e' la pigna viola di Cirmolo, la stessa del logo di avvio,
  su fondo notte; stesse dimensioni degli originali:
    tree_sm_close_crop.png  88x128, pigna su trasparente (icona dei messaggi e dello spegnimento)
    bg_tree_sm.png          640x480, pigna piccola al centro
    bg_tree.png             640x480, pigna grande appena visibile (sfondo dei messaggi)
    bg_tree_wide.png, bg_tree_sm_wide.png  854x480, le stesse per gli schermi 16:9
- Le altre (icone dorate, fondi dei messaggi) vengono ricolorate come il tema Cirmolo
  (tema_cirmolo.ricolora), partendo sempre dall'originale di spruceOS (tag v4.5.2), cosi' lo
  script si puo' rilanciare senza ricolorare due volte.

Uso: python -I .github/branding/immagini_sistema.py
"""
import io
import os
import subprocess
import sys

import numpy as np
from PIL import Image

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.abspath(os.path.join(QUI, '..', '..'))
sys.path.insert(0, QUI)
import tema_cirmolo as tc  # noqa: E402

IMGS = os.path.join(RADICE, 'spruce', 'imgs')
BASE = 'v4.5.2'
DISEGNATE = {'tree_sm_close_crop.png', 'bg_tree_sm.png', 'bg_tree.png', 'bg_tree_wide.png', 'bg_tree_sm_wide.png'}
INTATTE = {'splore.png'}                      # disegno di PICO-8, non e' grafica di spruce


def pigna(w, h, opacita=1.0):
    col, cop = tc.pigna_rgba(w, h)
    out = np.zeros((h, w, 4), np.float32)
    tc.incolla(out, col, cop * opacita, 0, 0)
    return out


def sfondo(w, h, icona_w, icona_h, opacita):
    out = np.dstack([tc.sfumatura(w, h), np.full((h, w, 1), 255, np.float32)])
    tc.incolla(out, *(lambda p: (p[..., :3], p[..., 3] / 255.0))(pigna(icona_w, icona_h, opacita)),
               (w - icona_w) // 2, (h - icona_h) // 2 - 8)
    return out


def salva(arr, nome, rgb=False):
    im = Image.fromarray(np.clip(np.round(arr), 0, 255).astype(np.uint8), 'RGBA')
    (im.convert('RGB') if rgb else im).save(os.path.join(IMGS, nome), optimize=True)


def originale(nome):
    dati = subprocess.run(['git', '-C', RADICE, 'show', '%s:spruce/imgs/%s' % (BASE, nome)],
                          capture_output=True, check=True).stdout
    return Image.open(io.BytesIO(dati))


def main():
    os.makedirs(IMGS, exist_ok=True)
    salva(pigna(88, 128), 'tree_sm_close_crop.png')
    salva(sfondo(640, 480, 96, 140, 1.0), 'bg_tree_sm.png')
    salva(sfondo(640, 480, 176, 304, 0.16), 'bg_tree.png')
    salva(sfondo(854, 480, 96, 140, 1.0), 'bg_tree_sm_wide.png', rgb=True)
    salva(sfondo(854, 480, 176, 304, 0.16), 'bg_tree_wide.png', rgb=True)
    n = 0
    for nome in sorted(os.listdir(IMGS)):
        if not nome.endswith('.png') or nome in DISEGNATE or nome in INTATTE:
            continue
        try:
            im = originale(nome)
        except subprocess.CalledProcessError:
            print('non in spruceOS %s, lascio stare: %s' % (BASE, nome))
            continue
        tc.ricolora(im).save(os.path.join(IMGS, nome), optimize=True)
        n += 1
    print('scritte 5 immagini con la pigna e ricolorate %d immagini in %s' % (n, IMGS))


if __name__ == '__main__':
    main()
