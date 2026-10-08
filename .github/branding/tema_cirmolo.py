#!/usr/bin/env python3
"""Genera il tema Cirmolo (Themes/Cirmolo) a partire dal tema SPRUCE di spruceOS.

Il tema SPRUCE (di tenlevels, icone di Icons8) usa la palette gruvbox: fondo grigio caldo, oro,
crema. Cirmolo prende la palette del logo di avvio: notte delle Dolomiti, viola della pigna,
verde degli aghi. Qui:
  - ogni immagine di skin/ e icons/ viene ricolorata: ogni pixel si proietta sul segmento tra due
    colori della palette di partenza e si riporta sullo stesso punto del segmento tra i due colori
    di arrivo, cosi' i bordi sfumati (anti-aliasing) restano puliti; trasparenza invariata;
  - gli sfondi a tinta unita diventano una sfumatura notturna leggerissima (con dithering, perche'
    lo schermo della Flip mostra le bande sui gradienti scuri);
  - dove spruce disegnava il suo albero (barra del titolo, animazioni di caricamento e di ricarica,
    salvataggio senza anteprima) c'e' la pigna di Cirmolo, la stessa del logo di avvio;
  - config.json: stessi caratteri e misure di SPRUCE, colori di Cirmolo.
Solo la risoluzione base 640x480 (quella della Flip): sugli altri schermi PyUI converte il tema da se'.

Uso:  python -I .github/branding/tema_cirmolo.py [--anteprima file.png]
"""
import argparse
import glob
import json
import os
import shutil
import sys

import numpy as np
from PIL import Image

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.abspath(os.path.join(QUI, '..', '..'))
sys.path.insert(0, QUI)
import logo_avvio as la  # noqa: E402

ORIGINE = os.path.join(RADICE, 'Themes', 'SPRUCE')
DESTINAZIONE = os.path.join(RADICE, 'Themes', 'Cirmolo')

# (colore in SPRUCE, colore in Cirmolo)
PALETTE = [
    ((40, 40, 40), (23, 26, 37)),         # fondo
    ((28, 32, 32), (15, 17, 26)),         # pannelli scuri
    ((60, 56, 52), (37, 40, 57)),         # pannelli
    ((80, 72, 68), (59, 52, 94)),         # riga selezionata, pulsanti attivi
    ((64, 64, 64), (50, 52, 70)),         # grigi neutri (icone delle console)
    ((84, 84, 84), (76, 78, 100)),
    ((128, 128, 128), (122, 122, 146)),
    ((20, 20, 20), (13, 14, 21)),
    ((124, 108, 100), (114, 109, 145)),   # icone non selezionate
    ((232, 216, 176), (236, 232, 248)),   # crema: testi chiari, campi
    ((212, 180, 92), (182, 156, 255)),    # oro: icone selezionate, app -> lavanda della pigna
    ((104, 156, 104), (110, 198, 142)),   # verde -> verde degli aghi
    ((212, 92, 12), (240, 150, 92)),      # arancione (batteria al 25%, cornici)
    ((204, 36, 28), (226, 72, 84)),       # rosso (batteria scarica)
    ((252, 252, 252), (252, 251, 255)),
    ((0, 0, 0), (0, 0, 0)),
]

COLORI = {
    'testo': '#ECE8F8',
    'testo_sel': '#F6F2FF',
    'spento': '#78739A',
    'lavanda': '#B69CFF',
    'aghi': '#7FD3A2',
    'notte': '#13151E',
}

# sfumatura degli sfondi: in alto un filo piu' chiaro, come l'alone del logo
FONDO_ALTO = np.array([27, 30, 43], np.float32)
FONDO_BASSO = np.array([18, 20, 29], np.float32)


def _segmenti():
    s = np.array([p[0] for p in PALETTE], np.float32)
    t = np.array([p[1] for p in PALETTE], np.float32)
    ii, jj = np.triu_indices(len(s), 0)          # coppie i<=j: i==j e' il colore singolo
    return s[ii], s[jj], t[ii], t[jj]


SA, SB, TA, TB = _segmenti()


def ricolora_colori(c):
    """c: N x 3 float. Restituisce i colori di Cirmolo corrispondenti."""
    d = SB - SA                                   # P x 3
    dd = (d * d).sum(1)
    dd[dd == 0] = 1
    rel = c[:, None, :] - SA[None]                # N x P x 3
    t = np.clip((rel * d[None]).sum(2) / dd[None], 0, 1)
    proiez = SA[None] + t[..., None] * d[None]
    dist = ((c[:, None, :] - proiez) ** 2).sum(2)
    k = dist.argmin(1)
    n = np.arange(len(c))
    tk = t[n, k][:, None]
    resto = c - proiez[n, k]
    return np.clip(TA[k] + tk * (TB[k] - TA[k]) + resto, 0, 255)


def ricolora(im):
    im = im.convert('RGBA')
    a = np.asarray(im).copy()
    rgb = a[..., :3].reshape(-1, 3)
    unici, inv = np.unique(rgb, axis=0, return_inverse=True)
    nuovi = np.round(ricolora_colori(unici.astype(np.float32))).astype(np.uint8)
    a[..., :3] = nuovi[inv.reshape(-1)].reshape(a.shape[:2] + (3,))
    return Image.fromarray(a, 'RGBA')


BAYER = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) + 0.5) / 16 - 0.5


def sfumatura(w, h, seme=0):
    """Sfumatura verticale con dithering ordinato (Bayer 4x4): niente bande e PNG piccoli, perche' il
    motivo si ripete (il rumore casuale del logo qui costerebbe ~300 KB per immagine)."""
    y = np.linspace(0, 1, h, dtype=np.float32)[:, None, None]
    img = FONDO_ALTO + (FONDO_BASSO - FONDO_ALTO) * (y ** 0.8)
    img = np.broadcast_to(img, (h, w, 3)).copy()
    img += np.tile(BAYER, (h // 4 + 1, w // 4 + 1))[:h, :w, None]
    return img


def fondo_unito(im):
    """Vero se l'immagine e' un fondo pieno #282828 largo quanto lo schermo (sfondi, barre)."""
    a = np.asarray(im.convert('RGBA'))
    return a.shape[1] >= 640 and (a[..., :3] == 40).all()


# fondi pieni che non partono dall'alto dello schermo: riga dove iniziano, cosi' prendono
# la stessa fetta di sfumatura e non si vede lo stacco con lo sfondo
FASCE = {'tips-bar-bg.png': 420, 'bg-gamelist.png': 60}


def pigna_rgba(w, h, palette=None):
    """La pigna del logo di avvio in un riquadro w x h: restituisce (colore, copertura) in float."""
    salva = (la.VIOLA_SCURO, la.VIOLA, la.VIOLA_CHIARO, la.SOLCO, la.DY)

    def render(pal):
        la.VIOLA_SCURO, la.VIOLA, la.VIOLA_CHIARO, la.SOLCO = [np.array(c, np.float32) for c in pal]
        la.DY = 0
        pad = 12
        img = np.zeros(((h + 2 * pad) * la.SS, (w + 2 * pad) * la.SS, 3), np.float32)
        la.pigna(img, w / 2.0 + pad, h / 2.0 + pad, w / 2.0 / 1.12 - 0.5, h / 2.0 - 0.5)
        img = img[pad * la.SS:(pad + h) * la.SS, pad * la.SS:(pad + w) * la.SS]
        im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), 'RGB').resize((w, h), Image.LANCZOS)
        return np.asarray(im, np.float32)

    try:
        copertura = render([(255, 255, 255)] * 4)[..., 0] / 255.0
        pal = palette or [salva[0], salva[1], salva[2], salva[3]]
        col = render(pal)
    finally:
        la.VIOLA_SCURO, la.VIOLA, la.VIOLA_CHIARO, la.SOLCO, la.DY = salva
    col = col / np.maximum(copertura, 1e-3)[..., None]
    return np.clip(col, 0, 255), copertura


SPENTA = [(44, 44, 58), (74, 72, 94), (104, 100, 128), (26, 26, 36)]


def incolla(fondo_rgba, col, cop, x0, y0):
    """Disegna la pigna (col, cop) su un'immagine RGBA float, con l'alfa giusta anche sul trasparente."""
    h, w = cop.shape
    reg = fondo_rgba[y0:y0 + h, x0:x0 + w]
    a0 = reg[..., 3:4] / 255.0
    a1 = cop[..., None]
    a = a1 + a0 * (1 - a1)
    rgb = (col * a1 + reg[..., :3] * a0 * (1 - a1)) / np.maximum(a, 1e-6)
    reg[..., :3] = rgb
    reg[..., 3:4] = a * 255


def salva_rgba(arr, path):
    Image.fromarray(np.clip(np.round(arr), 0, 255).astype(np.uint8), 'RGBA').save(path, optimize=True)


def barra_titolo(src, dst):
    """bg-title.png: la striscia in alto della sfumatura, con la pigna viola dove spruce aveva l'albero."""
    a = np.asarray(Image.open(src).convert('RGBA'), np.float32)
    h, w = a.shape[:2]
    fondo = a[h // 2, -5, :3]
    diff = np.abs(a[:, :200, :3] - fondo).sum(axis=2) > 30
    ys, xs = np.nonzero(diff)
    out = np.dstack([sfumatura(w, 480, 1)[:h], np.full((h, w, 1), 255, np.float32)])
    if len(xs):
        x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
        ph = y1 - y0 + 2
        pw = int(round(ph * 0.68))
        col, cop = pigna_rgba(pw, ph)
        incolla(out, col, cop, (x0 + x1) // 2 - pw // 2, y0 - 1)
    salva_rgba(out, dst)


def fotogrammi(dst_dir, prefisso):
    """app_loading_0N / charging-0N: la pigna spenta che si accende dal basso in 5 passi (spruce: l'albero)."""
    W, H = 100, 480
    ph, pw = 86, 58
    acc, cop = pigna_rgba(pw, ph)
    spe, _ = pigna_rgba(pw, ph, SPENTA)
    x0, y0 = (W - pw) // 2, 240 - ph // 2
    for n in range(1, 6):
        f = (n - 1) / 4.0
        soglia = ph * (1 - f)
        y = np.arange(ph, dtype=np.float32)[:, None, None]
        m = np.clip((y - soglia) / 3.0 + 0.5, 0, 1)
        col = spe * (1 - m) + acc * m
        out = np.zeros((H, W, 4), np.float32)
        incolla(out, col, cop, x0, y0)
        salva_rgba(out, os.path.join(dst_dir, '%s%02d.png' % (prefisso, n)))


def salvataggio_vuoto(src, dst):
    """save-default.png: anteprima di un salvataggio senza immagine. La pigna al posto dell'albero bianco."""
    w, h = Image.open(src).size
    out = np.zeros((h, w, 4), np.float32)
    ph = 92
    pw = int(round(ph * 0.68))
    col, cop = pigna_rgba(pw, ph)
    incolla(out, col, cop, (w - pw) // 2, (h - ph) // 2)
    salva_rgba(out, dst)


def config(src, dst):
    with open(src, encoding='utf-8') as f:
        c = json.load(f)
    c['name'] = 'Cirmolo'
    c['author'] = 'Cirmolo (dal tema Spruce di tenlevels)'
    c['description'] = 'Cirmolo theme for the Miyoo Flip: night, cembra pine cone and needles'
    c.pop('mainMenuTitle', None)        # SPRUCE lo lascia vuoto: cosi' vale quello di PyUI, «Cirmolo»
    for k in ('batteryPercentage', 'title', 'hint', 'currentpage'):
        c[k]['color'] = COLORI['testo']
    c['total']['color'] = COLORI['aghi']
    c['grid']['color'] = COLORI['spento']
    c['grid']['selectedcolor'] = COLORI['testo_sel']
    c['list']['color'] = COLORI['testo']
    c['list']['selectedcolor'] = COLORI['testo_sel']
    s = c['screensaver']
    s['bgColor'] = COLORI['notte']
    for wdg in s['widgets']:
        wdg['color'] = {'clock': '#FFFFFF', 'date': COLORI['lavanda'], 'battery': COLORI['aghi']}.get(wdg['type'], wdg['color'])
    with open(dst, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(c, f, ensure_ascii=False, indent=4)
        f.write('\n')


def icone_app():
    """Le icone che le app portano con se' (le app di Cirmolo, Songo...) e quelle delle aree del menu App
    sono dorate come il tema SPRUCE. PyUI cerca prima icons/app/<nome icona> nel tema attivo: qui ne
    mette la versione ricolorata, cosi' le app restano dorate con SPRUCE e diventano viola con Cirmolo."""
    dst_dir = os.path.join(DESTINAZIONE, 'icons', 'app')
    n = 0
    for cfg in sorted(glob.glob(os.path.join(RADICE, 'App', '*', 'config.json'))):
        try:
            with open(cfg, encoding='utf-8') as f:
                nome = json.load(f).get('icon')
        except (OSError, ValueError):
            continue
        if not nome or os.path.basename(nome) == 'icon.png':
            continue                                    # nome generico: il tema non puo' distinguerla
        nome = os.path.basename(nome)
        if os.path.exists(os.path.join(ORIGINE, 'icons', 'app', nome)):
            continue                                    # gia' fatta dal tema SPRUCE
        cartella = os.path.dirname(cfg)
        src = next((p for p in (os.path.join(cartella, nome), os.path.join(cartella, 'icon.png')) if os.path.isfile(p)), None)
        if src:
            ricolora(Image.open(src)).save(os.path.join(dst_dir, nome), optimize=True)
            n += 1
    for src in sorted(glob.glob(os.path.join(RADICE, 'App', 'PyUI', 'folder-icons', '*.png'))):
        ricolora(Image.open(src)).save(os.path.join(dst_dir, os.path.basename(src)), optimize=True)
        n += 1
    return n


CREDITI = """Cirmolo theme
=============
Generated by .github/branding/tema_cirmolo.py from the SPRUCE theme of spruceOS:
same layout, fonts and icons, recoloured with the palette of the Cirmolo boot logo,
and the Cirmolo pine cone where spruce drew its tree.

Credits of the original SPRUCE theme follow.

"""


def genera():
    if os.path.isdir(DESTINAZIONE):
        shutil.rmtree(DESTINAZIONE)
    os.makedirs(DESTINAZIONE)
    for nome in ('nunwen.ttf',):
        shutil.copy2(os.path.join(ORIGINE, nome), DESTINAZIONE)
    shutil.copytree(os.path.join(ORIGINE, 'sound'), os.path.join(DESTINAZIONE, 'sound'))
    with open(os.path.join(ORIGINE, 'CREDITS.txt'), encoding='utf-8', errors='replace') as f:
        crediti = f.read()
    with open(os.path.join(DESTINAZIONE, 'CREDITS.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(CREDITI + crediti)
    config(os.path.join(ORIGINE, 'config.json'), os.path.join(DESTINAZIONE, 'config.json'))
    # anteprima nella scelta del tema: una schermata vera del menu principale, presa dalla Flip
    shutil.copy2(os.path.join(QUI, 'schermate', 'menu.png'), os.path.join(DESTINAZIONE, 'preview.png'))

    n = 0
    for cartella in ('skin', 'icons'):
        for src in sorted(glob.glob(os.path.join(ORIGINE, cartella, '**', '*.png'), recursive=True)):
            rel = os.path.relpath(src, ORIGINE)
            dst = os.path.join(DESTINAZIONE, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            im = Image.open(src)
            if fondo_unito(im):
                alfa = np.asarray(im.convert('RGBA'), np.float32)[..., 3:4]
                h, w = alfa.shape[:2]
                y0 = FASCE.get(os.path.basename(src), 0)
                salva_rgba(np.dstack([sfumatura(w, max(480, y0 + h))[y0:y0 + h], alfa]), dst)
            else:
                ricolora(im).save(dst, optimize=True)
            n += 1
        for altro in glob.glob(os.path.join(ORIGINE, cartella, '**', '*'), recursive=True):
            if os.path.isfile(altro) and not altro.endswith('.png'):
                dst = os.path.join(DESTINAZIONE, os.path.relpath(altro, ORIGINE))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(altro, dst)

    n += icone_app()

    skin = os.path.join(DESTINAZIONE, 'skin')
    barra_titolo(os.path.join(ORIGINE, 'skin', 'bg-title.png'), os.path.join(skin, 'bg-title.png'))
    fotogrammi(skin, 'app_loading_')
    fotogrammi(skin, 'charging-')
    salvataggio_vuoto(os.path.join(ORIGINE, 'skin', 'save-default.png'), os.path.join(skin, 'save-default.png'))
    print('tema Cirmolo: %d immagini in %s' % (n, os.path.relpath(DESTINAZIONE, RADICE)))


def anteprima(out):
    """Foglio di controllo: le immagini piu' visibili del tema, su fondo a scacchi."""
    nomi = ['skin/background.png', 'skin/bg-title.png', 'skin/bg-list-s.png', 'skin/ic-app-f.png',
            'skin/ic-app-n.png', 'skin/ic-favorite-f.png', 'skin/ic-game-n.png', 'skin/ic-setting-n.png',
            'skin/app_loading_01.png', 'skin/app_loading_03.png', 'skin/app_loading_05.png',
            'skin/charging-05.png', 'skin/save-default.png', 'skin/icon-START.png', 'skin/icon-L2.png',
            'skin/ic-power-charge-25%.png', 'skin/ic-power-charge-100%.png', 'skin/list-item-select-bg-short.png',
            'icons/app/backup.png', 'icons/app/gallery.png', 'icons/gba.png', 'icons/psp.png']
    foglio = Image.new('RGBA', (1100, 980), (90, 90, 90, 255))
    x = y = 0
    riga = 0
    for nome in nomi:
        im = Image.open(os.path.join(DESTINAZIONE, nome)).convert('RGBA')
        if im.width > 640:
            im = im.resize((im.width // 2, im.height // 2))
        if x + im.width > foglio.width:
            x, y = 0, y + riga + 6
            riga = 0
        foglio.alpha_composite(im, (x, y))
        x += im.width + 6
        riga = max(riga, im.height)
    foglio.save(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--anteprima')
    args = ap.parse_args()
    genera()
    if args.anteprima:
        anteprima(args.anteprima)


if __name__ == '__main__':
    main()
