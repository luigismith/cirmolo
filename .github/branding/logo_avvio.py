#!/usr/bin/env python3
"""Genera il logo di avvio di Cirmolo: App/BootLogo/Imgs/Cirmolo.png (640x480).

La pigna viola del pino cembro (il cirmolo) tra due ciuffi di aghi, con la scritta «Cirmolo».
Disegno in supersampling 4x con numpy (la pigna e' uno shader per pixel: squame a losanga
sulla superficie di un ovoide, luce dall'alto a sinistra), poi riduzione Lanczos e un
leggerissimo dithering, perche' sui gradienti scuri lo schermo della Flip mostra le bande.

Font: Be Vietnam Pro SemiBold (SIL Open Font License 1.1), gia' incluso in App/PyUI/fonts.

Uso:  python -I .github/branding/logo_avvio.py [--out file.png]
"""
import argparse
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

RADICE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
FONT = os.path.join(RADICE, 'App', 'PyUI', 'fonts', 'BeVietnamPro-SemiBold.ttf')
OUT = os.path.join(RADICE, 'App', 'BootLogo', 'Imgs', 'Cirmolo.png')

W, H = 640, 480
SS = 4                      # supersampling
DY = 0                      # spostamento verticale di tutto il disegno (unita' 1x)

# Palette
SFONDO = (11, 14, 18)
ALONE = (29, 35, 47)
VIOLA_SCURO = np.array([34, 17, 70], np.float32)
VIOLA = np.array([104, 66, 192], np.float32)
VIOLA_CHIARO = np.array([182, 152, 255], np.float32)
SOLCO = np.array([20, 10, 40], np.float32)
VERDI = [  # (base, punta) dei ciuffi: dietro piu' scuri, davanti piu' chiari
    (np.array([22, 66, 48], np.float32), np.array([52, 122, 86], np.float32)),
    (np.array([30, 92, 64], np.float32), np.array([86, 172, 118], np.float32)),
    (np.array([42, 118, 80], np.float32), np.array([132, 214, 156], np.float32)),
]
TESTO = (236, 236, 244)
PUNTINO = (150, 116, 242)


def sfondo():
    h, w = H * SS, W * SS
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    cx, cy = w * 0.5, h * (0.40 + DY / H)
    d = np.hypot((x - cx) / w, (y - cy) / h)
    base, alone = np.array(SFONDO, np.float32), np.array(ALONE, np.float32)
    img = base + (alone - base) * np.exp(-(d / 0.30) ** 2)[..., None]
    r = np.hypot((x - w / 2) / (w / 2), (y - h / 2) / (h / 2))
    return img * np.clip(1.0 - 0.32 * (r - 0.65), 0.78, 1.0)[..., None]


def ago(img, p0, ang, lung, r0, r1, col0, col1):
    """Un ago: capsula affusolata da p0 (unita' 1x), angolo in gradi (90 = su), colore base->punta."""
    a = math.radians(ang)
    dx, dy = math.cos(a), -math.sin(a)
    x0, y0 = p0[0] * SS, (p0[1] + DY) * SS
    L, R0, R1 = lung * SS, r0 * SS, r1 * SS
    x1, y1 = x0 + dx * L, y0 + dy * L
    pad = R0 + 2
    xa, xb = int(max(0, min(x0, x1) - pad)), int(min(img.shape[1], max(x0, x1) + pad + 1))
    ya, yb = int(max(0, min(y0, y1) - pad)), int(min(img.shape[0], max(y0, y1) + pad + 1))
    yy, xx = np.mgrid[ya:yb, xa:xb].astype(np.float32) + 0.5
    px, py = xx - x0, yy - y0
    t = np.clip((px * dx + py * dy) / L, 0, 1)
    dist = np.hypot(px - dx * L * t, py - dy * L * t) - (R0 + (R1 - R0) * t)
    alfa = np.clip(0.5 - dist, 0, 1)[..., None]
    col = col0 + (col1 - col0) * (t ** 0.8)[..., None]
    sub = img[ya:yb, xa:xb]
    sub[:] = sub * (1 - alfa) + col * alfa


def ciuffo(img, origine, verso, strato, seme):
    """Ciuffo di aghi a ventaglio. verso -1 = verso sinistra, +1 = verso destra.
    Angoli regolari e lunghezze su un inviluppo morbido: un emblema, non un cespuglio."""
    rng = np.random.default_rng(seme)
    n = (13, 12, 9)[strato]
    a0, a1 = ((100, 196), (106, 190), (114, 182))[strato]
    sfasa = (0.0, 0.5, 0.25)[strato]
    for i in range(n):
        f = (i + sfasa) / (n - 1 + sfasa)
        ang = a0 + f * (a1 - a0) + rng.uniform(-1.2, 1.2)
        lung = (62 + 52 * math.sin(math.pi * (0.15 + 0.8 * f))) * (1.06, 0.95, 0.74)[strato] + rng.uniform(-2.5, 2.5)
        if verso > 0:
            ang = 180 - ang
        base, punta = VERDI[strato]
        k = rng.uniform(0.94, 1.05)
        ago(img, origine, ang, lung, (2.2, 2.1, 1.9)[strato], 0.35, base * k, np.minimum(punta * k, 255))


def ombra_pigna(img, cx, cy, rx, ry):
    """Ombra morbida attorno alla pigna, per staccarla dagli aghi."""
    m = Image.new('L', (W * SS, H * SS), 0)
    d = ImageDraw.Draw(m)
    g = 5
    d.ellipse([(cx - rx - g) * SS, (cy + DY - ry - g + 3) * SS, (cx + rx + g) * SS, (cy + DY + ry + g + 3) * SS], fill=255)
    m = m.filter(ImageFilter.GaussianBlur(7 * SS))
    a = np.asarray(m, np.float32)[..., None] / 255.0
    img *= 1 - 0.62 * a


def pigna(img, cx, cy, rx, ry):
    """Pigna di cembro: ovoide piu' largo in basso, squame a losanga in rilievo, luce dall'alto a sinistra."""
    cx, cy, rx, ry = cx * SS, (cy + DY) * SS, rx * SS, ry * SS
    ya, yb = int(cy - ry - 3), int(cy + ry + 4)
    xa, xb = int(cx - rx * 1.15 - 3), int(cx + rx * 1.15 + 4)
    yy, xx = np.mgrid[ya:yb, xa:xb].astype(np.float32) + 0.5
    t = (yy - cy) / ry
    rxy = rx * (1 + 0.11 * t)
    nx, ny = (xx - cx) / rxy, t
    r = np.sqrt(nx * nx + ny * ny)
    nz = np.sqrt(np.clip(1 - r * r, 0, 1))

    # coordinate sulla superficie (longitudine u, latitudine v) e reticolo di losanghe
    u = np.arcsin(np.clip(nx / np.sqrt(np.clip(1 - ny * ny, 1e-6, 1)), -1, 1))
    v = np.arcsin(np.clip(ny, -1, 1))
    du, dv = math.pi / 4.3, math.pi / 8.8
    a = u / du + v / dv + 0.5
    b = u / du - v / dv
    ka, kb = np.floor(a), np.floor(b)
    fa, fb = a - ka - 0.5, b - kb - 0.5

    # rilievo di ogni squama (apofisi): losanga piu' larga che alta, leggermente bombata,
    # con la carena orizzontale al centro e i solchi netti tra una squama e l'altra
    p = 5.0
    m = (np.abs(fa) ** p + np.abs(fb) ** p) ** (1 / p)
    piastra = np.clip((0.5 - m) / 0.13, 0, 1)
    piastra = piastra * piastra * (3 - 2 * piastra)
    lv = (fa - fb) / 2                                    # verticale dentro la squama (-0.5 su, +0.5 giu')
    cupola = 1 - (m / 0.5) ** 2
    carena = np.exp(-(lv / 0.07) ** 2)
    h = piastra * (0.72 + 0.20 * cupola + 0.10 * carena)
    gy, gx = np.gradient(h)
    k = 17.0
    n = np.stack([nx - k * gx * nz, ny - k * gy * nz, nz], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-6

    luce = np.array([-0.52, -0.62, 0.59], np.float32)
    luce /= np.linalg.norm(luce)
    diff = np.clip(n @ luce, 0, 1)
    meta = luce + np.array([0, 0, 1], np.float32)
    meta /= np.linalg.norm(meta)
    sfera = np.stack([nx, ny, nz], -1)
    lucido = np.clip(sfera @ meta, 0, 1) ** 9            # lucentezza ampia della pigna intera
    spec = np.clip(n @ meta, 0, 1) ** 40                  # piccoli riflessi sui bordi delle squame

    # la squama di sopra copre appena il margine superiore di questa
    ombra = np.clip((-lv - 0.30) / 0.2, 0, 1)

    # variazione naturale di tono da squama a squama
    rumore = np.sin(ka * 12.9898 + kb * 78.233) * 43758.5453
    rumore = rumore - np.floor(rumore)
    s = 0.15 + 0.85 * diff
    s *= 0.92 + 0.16 * rumore
    s *= 1 - 0.22 * ombra
    s *= 0.62 + 0.38 * nz ** 0.6                          # bordi piu' scuri
    s = np.clip(s, 0, 1)[..., None]
    col = np.where(s < 0.5, VIOLA_SCURO + (VIOLA - VIOLA_SCURO) * (s / 0.5),
                   VIOLA + (VIOLA_CHIARO - VIOLA) * ((s - 0.5) / 0.5))
    solco = np.clip(1 - piastra / 0.10, 0, 1)[..., None] * 0.85
    col = col * (1 - solco) + SOLCO * solco
    bianco = np.array([255, 238, 255], np.float32)
    col = col + bianco * (0.14 * lucido + 0.12 * spec * (piastra > 0.5))[..., None]

    # profilo dentellato: vicino al bordo i solchi tra le squame incidono il contorno
    bordo = (1 - r) * np.minimum(rxy, ry) - (1 - piastra) * 1.8 * SS * np.clip((r - 0.86) / 0.14, 0, 1)
    alfa = np.clip(bordo + 0.5, 0, 1)[..., None]
    sub = img[ya:yb, xa:xb]
    sub[:] = sub * (1 - alfa) + np.clip(col, 0, 255) * alfa


def _puntino_i(font):
    """Centro e misure del puntino della 'i' e centro dell'asta della 'ı', rispetto al punto di ancoraggio."""
    lato = int(font.size * 2)
    ox, oy = lato // 4, lato * 3 // 4
    misure = {}
    for c in ('i', 'ı'):
        im = Image.new('L', (lato, lato), 0)
        ImageDraw.Draw(im).text((ox, oy), c, font=font, fill=255, anchor='ls')
        misure[c] = np.asarray(im) > 127
    a = misure['i']
    righe = np.where(a.any(axis=1))[0]
    fine = righe[0]
    while fine + 1 < len(a) and a[fine + 1].any():
        fine += 1
    colonne = np.where(a[righe[0]:fine + 1].any(axis=0))[0]
    asta = np.where(misure['ı'].any(axis=0))[0]
    return ((asta[0] + asta[-1] + 1) / 2 - ox, (righe[0] + fine + 1) / 2 - oy,
            colonne[-1] + 1 - colonne[0], fine + 1 - righe[0])


def scritta(img, testo, cx, y_base, corpo, spaziatura):
    """Scritta con spaziatura tra le lettere. Le 'i' perdono il puntino: al suo posto una losanga viola."""
    font = ImageFont.truetype(FONT, int(corpo * SS))
    glifi = testo.replace('i', 'ı')
    larghezze = [font.getlength(c) for c in glifi]
    tot = sum(larghezze) + spaziatura * SS * (len(glifi) - 1)
    m = Image.new('L', (W * SS, H * SS), 0)
    d = ImageDraw.Draw(m)
    x = cx * SS - tot / 2
    y = (y_base + DY) * SS
    px, py, pw, ph = _puntino_i(font)
    losanghe = []
    for c, orig, lw in zip(glifi, testo, larghezze):
        d.text((x, y), c, font=font, fill=255, anchor='ls')
        if orig == 'i':
            losanghe.append((x + px, y + py, pw * 0.78, ph * 0.92))
        x += lw + spaziatura * SS
    a = np.asarray(m, np.float32)[..., None] / 255.0
    img[:] = img * (1 - a) + np.array(TESTO, np.float32) * a
    for lx, ly, mw, mh in losanghe:
        q = Image.new('L', (W * SS, H * SS), 0)
        ImageDraw.Draw(q).polygon([(lx, ly - mh), (lx + mw, ly), (lx, ly + mh), (lx - mw, ly)], fill=255)
        a = np.asarray(q, np.float32)[..., None] / 255.0
        img[:] = img * (1 - a) + np.array(PUNTINO, np.float32) * a


def disegna():
    img = sfondo()
    cx, cy, rx, ry = 320, 178, 46, 70
    for strato in range(3):
        ciuffo(img, (cx - 16, cy + 50), -1, strato, 10 + strato)
        ciuffo(img, (cx + 16, cy + 50), +1, strato, 20 + strato)
    ombra_pigna(img, cx, cy, rx, ry)
    pigna(img, cx, cy, rx, ry)
    scritta(img, 'Cirmolo', 320, 343, 54, 2.5)
    out = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), 'RGB').resize((W, H), Image.LANCZOS)
    a = np.asarray(out, np.float32)
    a += np.random.default_rng(1).uniform(-0.75, 0.75, a.shape)   # dithering contro le bande
    return Image.fromarray(np.clip(np.round(a), 0, 255).astype(np.uint8), 'RGB')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=OUT)
    args = ap.parse_args()
    img = disegna()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    img.save(args.out, optimize=True)
    print('scritto %s (%dx%d)' % (args.out, img.width, img.height))


if __name__ == '__main__':
    main()
