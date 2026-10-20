#!/usr/bin/env python3
"""Le risorse del sito di Cirmolo (ramo gh-pages, https://luigismith.github.io/cirmolo/) nel tema STAME,
quello di serie della console: il sito si veste come lo schermo.

Scrive nella cartella del sito (predefinita: ../cirmolo-sito, un clone del ramo gh-pages):
  - fonts/: Matrice e Spleen 12x24 e 8x16 in WOFF2, ridotti all'alfabeto latino, dai TTF di
    Themes/STAME (generati da tema_stame.py), con le licenze (LICENSE-fonts.txt);
  - img/pigna.png e img/pigna-piccola.png: la pigna del logo di Cirmolo disegnata a inchiostro, come su
    un LCD (squame nell'inchiostro viola, aghi nel verde del fuoco), con la geometria di logo_avvio.py;
  - favicon.png e img/copertina.png (l'anteprima per i link condivisi, 1200x630; la stessa immagine
    va anche in schermate/copertina.png, in cima ai README);
  - img/stame.png: il marchio dei tre stami, per la scheda che rimanda a Stame (dal repository di Stame,
    come per il tema: ../stame oppure --stame);
  - img/: le schermate del tema prese dalla Flip e la tavola dei caratteri (schermate/, qui accanto).

Serve fontTools con brotli (python -m pip install fonttools brotli).
Uso:  python -I .github/branding/sito_stame.py [--sito ../cirmolo-sito] [--stame ../stame]
"""
import argparse
import os
import shutil
import sys

import numpy as np
from fontTools import subset
from PIL import Image, ImageDraw, ImageFont

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.abspath(os.path.join(QUI, '..', '..'))
sys.path.insert(0, QUI)
import logo_avvio as lg  # noqa: E402
import tema_stame as ts  # noqa: E402

SITO = os.path.join(os.path.dirname(RADICE), 'cirmolo-sito')
TEMA = ts.DESTINAZIONE
SCHERMATE = ('menu', 'app', 'musica', 'giochi', 'salvaschermo', 'caratteri')

# latino, punteggiatura, frecce e il blocco pieno (le cifre spente di Matrice)
LATINO = (list(range(0x20, 0x7F)) + list(range(0xA0, 0x180)) + list(range(0x2010, 0x2028))
          + list(range(0x2030, 0x203B)) + [0x20AC, 0x2122, 0x2190, 0x2191, 0x2192, 0x2193, 0x2212, 0x2588])
CARATTERI = {'matrice.woff2': ts.MATRICE_TTF, 'spleen-12x24.woff2': ts.SPLEEN_GRANDE,
             'spleen-8x16.woff2': ts.SPLEEN_PICCOLO}


def caratteri_web(sito):
    cartella = os.path.join(sito, 'fonts')
    os.makedirs(cartella, exist_ok=True)
    for nome, ttf in CARATTERI.items():
        opzioni = subset.Options()
        opzioni.flavor = 'woff2'
        opzioni.hinting = True          # il prep che tiene lontano l'autohinter
        opzioni.name_IDs = ['*']
        opzioni.notdef_outline = True
        font = subset.load_font(os.path.join(TEMA, ttf), opzioni)
        s = subset.Subsetter(opzioni)
        s.populate(unicodes=LATINO)
        s.subset(font)
        subset.save_font(font, os.path.join(cartella, nome), opzioni)
    shutil.copy2(os.path.join(TEMA, 'LICENSE-fonts.txt'), os.path.join(cartella, 'LICENSE-fonts.txt'))


def _ciuffi(strato, cx, cy):
    """Copertura (0-1) degli aghi di uno strato, con la stessa geometria del logo di avvio."""
    m = np.zeros((lg.H * lg.SS, lg.W * lg.SS, 3), np.float32)
    bianco = np.full(3, 255.0, np.float32)
    ago = lg.ago
    lg.ago = lambda img, p0, ang, lung, r0, r1, c0, c1: ago(img, p0, ang, lung, r0, r1, bianco, bianco)
    try:
        lg.ciuffo(m, (cx - 16, cy + 50), -1, strato, 10 + strato)
        lg.ciuffo(m, (cx + 16, cy + 50), +1, strato, 20 + strato)
    finally:
        lg.ago = ago
    return m[..., 0] / 255.0


def pigna(altezza):
    """La pigna a inchiostro, su fondo trasparente: solchi pieni e squame a meta' inchiostro (piu'
    scure sotto la squama di sopra), aghi pieni davanti e piu' chiari dietro, un filo di vetro fra la
    pigna e gli aghi."""
    cx, cy, rx, ry = 320, 178, 46, 70
    h, w = lg.H * lg.SS, lg.W * lg.SS
    (ya, yb, xa, xb), _, _, nz, _, _, _, piastra, lv, alfa = lg.forma_pigna(cx, cy, rx, ry)
    cono = np.zeros((h, w), np.float32)
    cono[ya:yb, xa:xb] = alfa[..., 0]
    dens = np.zeros((h, w), np.float32)
    ombra = np.clip((-lv - 0.30) / 0.2, 0, 1)
    squama = np.clip(0.38 + 0.22 * ombra + 0.24 * (1 - nz ** 0.5), 0, 1)    # piu' scura verso il bordo
    dens[ya:yb, xa:xb] = squama + (1 - piastra) * (1 - squama)
    alone = np.asarray(Image.fromarray((cono * 255).astype(np.uint8)).resize((w // 4, h // 4), Image.BILINEAR)
                       .resize((w, h), Image.BILINEAR), np.float32) / 255.0
    alone = np.clip(alone * 3, 0, 1)
    out = np.zeros((h, w, 4), np.float32)
    for strato, d in enumerate((0.5, 0.75, 1.0)):
        a = (_ciuffi(strato, cx, cy) * d * (1 - alone))[..., None]
        out[..., :3] = out[..., :3] * (1 - a) + ts.rgbf('focus') * a
        out[..., 3:] = out[..., 3:] * (1 - a) + a
    a = (cono * dens)[..., None]
    out[..., :3] = out[..., :3] * (1 - a) + ts.rgbf('loop') * a
    out[..., 3:] = out[..., 3:] * (1 - a) + a
    ys, xs = np.nonzero(out[..., 3] > 0.01)
    out = out[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    rgb = np.where(out[..., 3:] > 0, out[..., :3] / np.maximum(out[..., 3:], 1e-6), 0)
    im = Image.fromarray(np.clip(np.round(np.dstack([rgb, out[..., 3:] * 255])), 0, 255).astype(np.uint8), 'RGBA')
    # media dei pixel, non Lanczos: sul contorno trasparente Lanczos lascia un alone chiaro
    return im.convert('RGBa').resize((round(im.width * altezza / im.height), altezza), Image.BOX).convert('RGBA')


def favicon(dst):
    im = Image.new('RGBA', (64, 64), ts.rgb('bg') + (255,))
    p = pigna(36)
    im.alpha_composite(p, ((64 - p.width) // 2, (64 - p.height) // 2))
    im.save(dst, optimize=True)


def copertina(dst):
    """L'anteprima dei link: il vetro con le squadre, la pigna e il nome in Matrice."""
    img = ts.vetro(1200, 630)
    ts.squadre(img, ts.rgbf('ghost'), lato=22, margine=14)
    im = Image.fromarray(np.clip(np.round(img), 0, 255).astype(np.uint8), 'RGB').convert('RGBA')
    p = pigna(300)
    im.alpha_composite(p, (330 - p.width // 2, 315 - p.height // 2))
    d = ImageDraw.Draw(im)
    spleen = ImageFont.truetype(os.path.join(TEMA, ts.SPLEEN_GRANDE), 48)
    piccolo = ImageFont.truetype(os.path.join(TEMA, ts.SPLEEN_PICCOLO), 32)
    d.text((34, 26), 'TAB. I', font=piccolo, fill=ts.rgb('dim'))
    d.text((640, 330), 'Cirmolo', font=ImageFont.truetype(os.path.join(TEMA, ts.MATRICE_TTF), 132),
           fill=ts.rgb('treble'), anchor='ls')
    d.text((644, 400), 'AI & Music OS', font=spleen, fill=ts.rgb('text'), anchor='ls')
    d.text((644, 452), 'for handhelds', font=spleen, fill=ts.rgb('text'), anchor='ls')
    d.text((644, 520), 'Pinus cembra L.', font=piccolo, fill=ts.rgb('dim'), anchor='ls')
    im.convert('RGB').save(dst, optimize=True)


def marchio_stame(stame, dst, altezza=128):
    parti = ts.marchio(stame)
    col, cop = ts.disegna_marchio(parti, altezza, [ts.rgb('treble')] * 4)
    out = np.zeros(cop.shape + (4,), np.float32)
    ts.tc.incolla(out, col, cop, 0, 0)
    ts.tc.salva_rgba(out, dst)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--sito', default=SITO, help='clone del ramo gh-pages (predefinito: ../cirmolo-sito)')
    ap.add_argument('--stame', default=ts.STAME, help='cartella del repository di Stame (predefinita: ../stame)')
    args = ap.parse_args()
    sito = os.path.abspath(args.sito)
    if not os.path.isfile(os.path.join(sito, 'index.html')):
        sys.exit('non trovo il sito in %s' % sito)
    caratteri_web(sito)
    img = os.path.join(sito, 'img')
    for nome, altezza in (('pigna.png', 320), ('pigna-piccola.png', 64)):
        pigna(altezza).quantize(256, Image.Quantize.FASTOCTREE).save(os.path.join(img, nome), optimize=True)
    favicon(os.path.join(sito, 'favicon.png'))
    copertina(os.path.join(img, 'copertina.png'))
    shutil.copy2(os.path.join(img, 'copertina.png'), os.path.join(QUI, 'schermate', 'copertina.png'))
    marchio_stame(os.path.abspath(args.stame), os.path.join(img, 'stame.png'))
    for nome in SCHERMATE:
        shutil.copy2(os.path.join(QUI, 'schermate', nome + '.png'), os.path.join(img, nome + '.png'))
    print('sito: caratteri, pigna, favicon, copertina, marchio di Stame e %d schermate in %s' % (len(SCHERMATE), sito))


if __name__ == '__main__':
    main()
