#!/usr/bin/env python3
"""Genera il tema STAME (Themes/STAME), il tema di serie di Cirmolo: la plancia di Stame nel suo tema
Giorno, un LCD riflettente (vetro grigio-verde con la maglia, inchiostri scuri, colori con un ruolo).

Dal repository di Stame (https://github.com/luigismith/stame, MIT; atteso accanto a questo, in ../stame,
oppure con --stame) prende solo le risorse, mai il codice:
  - i colori del tema Giorno (docs/identita/README.md, par. 3.3), ricopiati qui sotto;
  - Matrice, il carattere a punti 5x7: la tabella GLIFI di tools/matrice.py, letta come dato;
  - Spleen 8x16 e 12x24 di Frederic Cambus (BSD 2-Clause): le tabelle di src/gfx/font_spleen_*.c
    e third_party/spleen/LICENSE;
  - il marchio dei tre stami, ritagliato da docs/identita/img/avvio_3.png.
Il suono dei tasti (sound/change.wav) e' il «granello di Stame»: il Mi5 del logo sonoro, una nota sola,
con lo stesso strumento. Il copione per SoundCraft e' stame/granello.sc.txt, qui accanto; il file
stame/granello.wav e' il suo bounce (soundcraft-cli script granello.sc.txt, nella cartella stame/).

Come il tema Cirmolo parte dal tema SPRUCE di spruceOS, ma:
  - le immagini dell'interfaccia si ricolorano con la luce rovesciata: il fondo scuro diventa il vetro
    chiaro, il crema diventa inchiostro, l'oro diventa l'acqua di Stame (verde scuro dove l'immagine
    e' quella della voce scelta, *-f.png);
  - le icone delle console diventano disegni a inchiostro, come su un LCD, e i loro colori vivi
    prendono i colori dei ruoli di Stame;
  - i fondi pieni diventano il vetro con la maglia; la testata e' un pannello con un filo sotto;
  - la riga scelta e' una cella scura con il segno del fuoco a sinistra;
  - dove spruce disegnava l'albero c'e' il marchio di Stame, e nelle animazioni di caricamento le
    antere si accendono una alla volta, come all'avvio di Stame;
  - caratteri: Matrice nel titolo della testata e nell'orologio del salvaschermo, Spleen in tutto il
    resto. PyUI non ha un carattere di riserva: ogni TTF prende da nunwen.ttf (il carattere di SPRUCE,
    SIL OFL 1.1) i glifi che non ha, cosi' anche i nomi dei giochi in giapponese o in cirillico si leggono.
Solo la risoluzione base 640x480 (quella della Flip): sugli altri schermi PyUI converte il tema da se'.

Serve fontTools (python -m pip install fonttools).
Uso:  python -I .github/branding/tema_stame.py [--stame ../stame] [--anteprima foglio.png]
"""
import argparse
import ast
import copy
import glob
import json
import math
import os
import re
import shutil
import sys
import unicodedata
import wave

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from fontTools.fontBuilder import FontBuilder
from fontTools.misc.timeTools import timestampSinceEpoch
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont, newTable
from fontTools.ttLib.tables import ttProgram
from fontTools.ttLib.tables._g_l_y_f import Glyph, GlyphCoordinates

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.abspath(os.path.join(QUI, '..', '..'))
sys.path.insert(0, QUI)
import tema_cirmolo as tc  # noqa: E402

ORIGINE = os.path.join(RADICE, 'Themes', 'SPRUCE')
DESTINAZIONE = os.path.join(RADICE, 'Themes', 'STAME')
RISORSE = os.path.join(QUI, 'stame')
STAME = os.path.join(os.path.dirname(RADICE), 'stame')

# Tema Giorno di Stame (docs/identita/README.md, par. 3.3)
GIORNO = {
    'bg': '#CED4C2',         # vetro
    'mesh': '#C8CEBB',       # maglia del vetro
    'panel': '#D9DECE',      # intestazioni e liste
    'line': '#A2AA95',       # separatori
    'seg_off': '#C1C8B4',    # segmenti e punti spenti
    'bar': '#B5BDA7',        # barre e celle
    'ghost': '#879180',      # legende dei comandi
    'text': '#141B18',       # valori accesi
    'dim': '#424C47',        # scritte stampate (sabbia)
    'treble': '#005A51',     # acqua
    'bass': '#853A00',       # ambra
    'loop': '#4A2894',       # viola
    'focus': '#285C00',      # fuoco
    'active': '#634A00',     # evidenze
    'warn': '#9E160C',       # avvisi
    'kb_white': '#E3E7D8',   # tasti bianchi spenti
}


def rgb(nome):
    h = GIORNO[nome]
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def rgbf(nome):
    return np.array(rgb(nome), np.float32)


# (colore in SPRUCE, colore in STAME): la luce si rovescia, cio' che era chiaro diventa inchiostro
PALETTE = [
    ((40, 40, 40), rgb('bg')),            # fondo -> vetro
    ((28, 32, 32), rgb('panel')),         # pannelli scuri -> pannelli chiari
    ((60, 56, 52), rgb('seg_off')),       # pannelli
    ((80, 72, 68), rgb('bar')),           # riga selezionata, pulsanti attivi -> cella
    ((64, 64, 64), rgb('seg_off')),       # grigi neutri
    ((84, 84, 84), rgb('bar')),
    ((128, 128, 128), rgb('ghost')),
    ((20, 20, 20), rgb('panel')),
    ((124, 108, 100), rgb('ghost')),      # icone non selezionate
    ((232, 216, 176), rgb('text')),       # crema: testi, campi -> inchiostro
    ((212, 180, 92), rgb('treble')),      # oro: icone, app -> acqua
    ((104, 156, 104), rgb('focus')),      # verde
    ((212, 92, 12), rgb('bass')),         # arancione -> ambra
    ((204, 36, 28), rgb('warn')),         # rosso
    ((252, 252, 252), rgb('text')),
    ((0, 0, 0), rgb('kb_white')),
]
SEGMENTI = tc.segmenti(PALETTE)
# l'immagine della voce scelta (*-f.png): l'oro prende il colore del fuoco
SEGMENTI_FUOCO = tc.segmenti([(s, rgb('focus')) if s == (212, 180, 92) else (s, t) for s, t in PALETTE])
# cornici di scelta di spruce (fondo scuro con il bordo arancione): cella scura e bordo del fuoco
SEGMENTI_SCELTA = tc.segmenti([((28, 32, 32), rgb('bar')), ((124, 108, 100), rgb('bar')),
                               ((212, 92, 12), rgb('focus')), ((178, 171, 165), rgb('ghost')),
                               ((0, 0, 0), rgb('bar'))])
SCELTA = ('list-item-select-bg-short.png', 'bg-ra-list-item.png', 'grid-game-selected.png')

MATRICE_TTF = 'stame-matrice.ttf'
SPLEEN_GRANDE = 'stame-spleen-12x24.ttf'
SPLEEN_PICCOLO = 'stame-spleen-8x16.ttf'


# ---------------------------------------------------------------------------------------------------
# Caratteri

def glifi_matrice(stame):
    """La tabella GLIFI di tools/matrice.py di Stame, letta come dato (ast), senza eseguire il file."""
    with open(os.path.join(stame, 'tools', 'matrice.py'), encoding='utf-8') as f:
        albero = ast.parse(f.read())
    for nodo in albero.body:
        if isinstance(nodo, ast.Assign) and any(getattr(t, 'id', None) == 'GLIFI' for t in nodo.targets):
            return ast.literal_eval(nodo.value)
    raise SystemExit('tools/matrice.py di Stame: tabella GLIFI non trovata')


def spleen(stame, misura):
    """Spleen dalle tabelle di src/gfx/font_spleen_<misura>.c: ({cp: [righe di pixel]}, w, h, ascent)."""
    with open(os.path.join(stame, 'src', 'gfx', 'font_spleen_%s.c' % misura), encoding='utf-8') as f:
        testo = re.sub(r'/\*.*?\*/', '', f.read(), flags=re.S)
    w, h, ascent, bpr, n = map(int, re.search(
        r'gfx_font_t\s+\w+\s*=\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)', testo).groups())
    blocco = lambda nome: re.search(nome + r'\[\d+\][^{]*\{(.*?)\};', testo, re.S).group(1)  # noqa: E731
    cps = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]+)', blocco('CP'))]
    bits = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]+)', blocco('BITS'))]
    assert len(cps) == n and len(bits) == n * h * bpr, misura
    glifi = {}
    for i, cp in enumerate(cps):
        righe = []
        for r in range(h):
            v = int.from_bytes(bytes(bits[(i * h + r) * bpr:(i * h + r + 1) * bpr]), 'big')
            righe.append(''.join('#' if v >> (bpr * 8 - 1 - c) & 1 else '.' for c in range(w)))
        glifi[cp] = righe
    return glifi, w, h, ascent


# Glifi che Matrice non ha, disegnati qui nel suo stile (righe 0-6 sopra la linea di base, 7-8 sotto):
# la punteggiatura ASCII che manca e i segni che l'italiano usa nei menu.
MATRICE_IN_PIU = {
    '"': ['#.#', '#.#', '...', '...', '...', '...', '...'],
    '$': ['..#..', '.####', '#.#..', '.###.', '..#.#', '####.', '..#..'],
    '&': ['.##..', '#..#.', '#.#..', '.#...', '#.#.#', '#..#.', '.##.#'],
    '*': ['.....', '..#..', '#.#.#', '.###.', '#.#.#', '..#..', '.....'],
    ',': ['..', '..', '..', '..', '..', '##', '##', '.#', '#.'],
    ';': ['..', '##', '##', '..', '##', '##', '.#', '#.', '..'],
    '<': ['...#', '..#.', '.#..', '#...', '.#..', '..#.', '...#'],
    '=': ['....', '....', '####', '....', '####', '....', '....'],
    '>': ['#...', '.#..', '..#.', '...#', '..#.', '.#..', '#...'],
    '@': ['.###.', '#...#', '#.###', '#.#.#', '#.###', '#....', '.####'],
    '[': ['###', '#..', '#..', '#..', '#..', '#..', '###'],
    '\\': ['#...', '#...', '.#..', '.#..', '..#.', '..#.', '...#'],
    ']': ['###', '..#', '..#', '..#', '..#', '..#', '###'],
    '^': ['..#..', '.#.#.', '#...#', '.....', '.....', '.....', '.....'],
    '_': ['.....', '.....', '.....', '.....', '.....', '.....', '#####'],
    '`': ['#.', '.#', '..', '..', '..', '..', '..'],
    '{': ['..#', '.#.', '.#.', '#..', '.#.', '.#.', '..#'],
    '|': ['#', '#', '#', '#', '#', '#', '#'],
    '}': ['#..', '.#.', '.#.', '..#', '.#.', '.#.', '#..'],
    '~': ['.....', '.....', '.#...', '#.#.#', '...#.', '.....', '.....'],
    '¡': ['#', '.', '#', '#', '#', '#', '#'],
    '¿': ['..#..', '.....', '..#..', '.#...', '#....', '#...#', '.###.'],
    '«': ['.....', '..#.#', '.#.#.', '#.#..', '.#.#.', '..#.#', '.....'],
    '»': ['.....', '#.#..', '.#.#.', '..#.#', '.#.#.', '#.#..', '.....'],
    '·': ['.', '.', '.', '#', '.', '.', '.'],
    '×': ['.....', '#...#', '.#.#.', '..#..', '.#.#.', '#...#', '.....'],
    '÷': ['.....', '..#..', '.....', '#####', '.....', '..#..', '.....'],
    '±': ['..#..', '..#..', '#####', '..#..', '..#..', '.....', '#####'],
    '£': ['..##.', '.#..#', '.#...', '###..', '.#...', '.#..#', '#.##.'],
    '€': ['..###', '.#...', '####.', '.#...', '####.', '.#...', '..###'],
    'ß': ['.##.', '#..#', '#..#', '#.#.', '#..#', '#..#', '#.#.'],
    'Æ': ['.####', '#.#..', '#.#..', '#####', '#.#..', '#.#..', '#.###'],
    'æ': ['.....', '.....', '##.#.', '..###', '.####', '#.#..', '.#.##'],
    'Ø': ['.###.', '#..##', '#..##', '#.#.#', '##..#', '##..#', '.###.'],
    'ø': ['.....', '.....', '.###.', '#..##', '#.#.#', '##..#', '.###.'],
    '—': ['.....', '.....', '.....', '#####', '.....', '.....', '.....'],
    '…': ['.....', '.....', '.....', '.....', '.....', '.....', '#.#.#'],
    '•': ['...', '...', '###', '###', '###', '...', '...'],
    '█': ['#####'] * 7,   # la cella piena: i punti spenti dell'orologio nel salvaschermo
}
# segni che hanno gia' un gemello: l'apostrofo tipografico e' l'apostrofo, e cosi' via
GEMELLI = {'’': "'", '‘': "'", '‚': ',', '“': '"', '”': '"', '„': '"', '–': '-', '‐': '-', '−': '-',
           ' ': ' ', ' ': ' '}
# accenti, due righe sopra la lettera (o sotto, per cediglia e ogonek)
ACCENTI = {
    '̀': ['#.', '.#'], '́': ['.#', '#.'], '̂': ['.#.', '#.#'], '̃': ['.#.#', '#.#.'],
    '̄': ['...', '###'], '̆': ['#.#', '.#.'], '̇': ['.', '#'], '̈': ['...', '#.#'],
    '̊': ['##', '##'], '̋': ['.#.#', '#.#.'], '̌': ['#.#', '.#.'],
    '̧': ['.#', '#.'], '̨': ['#.', '.#'],
}
SOTTO = ('̧', '̨')


def matrice_completa(glifi):
    """{carattere: (larghezza, insieme di punti (riga, colonna))}: Matrice, i glifi in piu' e le lettere
    accentate composte dalla base e dall'accento (latino-1 e latino esteso A)."""
    tab = {}
    for ch, righe in list(glifi.items()) + list(MATRICE_IN_PIU.items()):
        punti = {(r, c) for r, riga in enumerate(righe) for c, x in enumerate(riga) if x == '#'}
        tab.setdefault(ch, (len(righe[0]), punti))
    for ch, gemello in GEMELLI.items():
        tab.setdefault(ch, tab[gemello])
    for cp in list(range(0xC0, 0x250)):
        ch = chr(cp)
        nfd = unicodedata.normalize('NFD', ch)
        if ch in tab or len(nfd) != 2 or nfd[0] not in tab or nfd[1] not in ACCENTI:
            continue
        w, base = tab[nfd[0]]
        if nfd[0] in 'ij':
            base = {(r, c) for r, c in base if r > 0}          # i e j senza il punto
        acc = ACCENTI[nfd[1]]
        wa = len(acc[0])
        x0 = max(0, (w - wa) // 2)
        if nfd[1] in SOTTO:
            r0 = 7
        else:
            r0 = -1 if not any(r < 2 for r, _ in base) else -2   # minuscole: sopra l'occhio; maiuscole: sopra la cima
        punti = set(base) | {(r0 + i, x0 + j) for i, riga in enumerate(acc) for j, x in enumerate(riga) if x == '#'}
        tab[ch] = (max(w, x0 + wa), punti)
    return tab


class Ttf:
    """Un TTF a contorni costruito glifo per glifo, piu' i glifi di riserva presi da un altro TTF."""

    def __init__(self, upem, ascent, descent):
        self.upem, self.ascent, self.descent = upem, ascent, descent
        self.ordine, self.glifi, self.metriche, self.cmap = [], {}, {}, {}
        u = upem // 16

        def tofu(pen):          # .notdef: il riquadro vuoto
            for x0, y0, x1, y1 in ((u, 0, 7 * u, ascent - u),):
                pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()
                pen.moveTo((x0 + u, y0 + u)); pen.lineTo((x1 - u, y0 + u)); pen.lineTo((x1 - u, y1 - u)); pen.lineTo((x0 + u, y1 - u)); pen.closePath()
        self._metti('.notdef', 8 * u, tofu)

    def _metti(self, nome, avanzamento, disegna=None, glifo=None):
        if glifo is None:
            pen = TTGlyphPen(None)
            disegna(pen)
            glifo = pen.glyph()
        glifo.recalcBounds(self.glifi)
        self.ordine.append(nome)
        self.glifi[nome] = glifo
        self.metriche[nome] = (int(avanzamento), getattr(glifo, 'xMin', 0) if glifo.numberOfContours else 0)

    def aggiungi(self, cp, avanzamento, disegna):
        nome = 'uni%04X' % cp if cp <= 0xFFFF else 'u%05X' % cp
        self._metti(nome, avanzamento, disegna)
        self.cmap[cp] = nome

    def riserva(self, sorgente, scala):
        """Copia da sorgente (TTFont) i caratteri che mancano, in scala, senza istruzioni di hinting.
        I glifi composti restano composti (in nunwen sono quasi tutti): i pezzi si copiano una volta."""
        glyf, hmtx = sorgente['glyf'], sorgente['hmtx']
        copiati = {}

        def copia(gn):
            if gn in copiati:
                return copiati[gn]
            g = glyf[gn]
            nuovo = Glyph()
            if g.isComposite() and all(hasattr(c, 'x') for c in g.components):
                pezzi = []
                for c in g.components:
                    pezzo = copy.copy(c)
                    pezzo.glyphName = copia(c.glyphName)
                    pezzo.x, pezzo.y = round(c.x * scala), round(c.y * scala)
                    pezzo.flags &= ~0x0100                        # niente istruzioni
                    pezzi.append(pezzo)
                nuovo.numberOfContours = -1
                nuovo.components = pezzi
            elif g.numberOfContours:
                coords, fini, flags = g.getCoordinates(glyf)
                if len(coords):
                    coords = GlyphCoordinates(coords)
                    coords.scale((scala, scala))
                    coords.toInt()
                    nuovo.numberOfContours = len(fini)
                    nuovo.coordinates = coords
                    nuovo.endPtsOfContours = list(fini)
                    nuovo.flags = bytearray(f & 1 for f in flags)
                    nuovo.program = ttProgram.Program()
                    nuovo.program.fromBytecode(b'')
            nome = 'r%05d' % len(copiati)
            self._metti(nome, round(hmtx[gn][0] * scala), glifo=nuovo)
            copiati[gn] = nome
            return nome

        for cp, gn in sorted(sorgente.getBestCmap().items()):
            if cp not in self.cmap and cp >= 0x20:
                self.cmap[cp] = copia(gn)

    def salva(self, percorso, famiglia, copyright_, maiuscola):
        fb = FontBuilder(self.upem, isTTF=True)
        fb.setupGlyphOrder(self.ordine)
        fb.setupCharacterMap(self.cmap)
        fb.setupGlyf(self.glifi)
        fb.setupHorizontalMetrics(self.metriche)
        fb.setupHorizontalHeader(ascent=self.ascent, descent=-self.descent)
        fb.setupNameTable({
            'familyName': famiglia, 'styleName': 'Regular', 'copyright': copyright_,
            'licenseDescription': 'This Font Software is licensed under the SIL Open Font License, Version 1.1. '
                                  'See LICENSE-fonts.txt in the STAME theme of Cirmolo.',
            'licenseInfoURL': 'https://openfontlicense.org',
        })
        fb.setupOS2(sTypoAscender=self.ascent, sTypoDescender=-self.descent, sTypoLineGap=0,
                    usWinAscent=self.ascent, usWinDescent=self.descent, sCapHeight=maiuscola,
                    achVendID='CIRM', fsType=0)
        fb.setupPost(keepGlyphNames=False)
        f = fb.font
        f['head'].flags |= 1 << 3                       # ppem intero: i pixel cadono sui pixel
        # un programma «prep» (controllo delle sbavature, come nei caratteri bitmap) basta perche'
        # FreeType non applichi l'auto-hinter, che sposterebbe i pixel dei glifi
        prep = newTable('prep')
        prep.program = ttProgram.Program()
        prep.program.fromAssembly('PUSHW[ ]\n511\nSCANCTRL[ ]\nPUSHB[ ]\n4\nSCANTYPE[ ]')
        f['prep'] = prep
        f['maxp'].maxZones = 2
        f['maxp'].maxStackElements = 8
        # data fissa (19/10/2026): a ogni generazione gli stessi byte, e git non vede cambiamenti finti
        f['head'].created = f['head'].modified = timestampSinceEpoch(1792368000)
        f.recalcTimestamp = False
        f.save(percorso)


def scala_riserva(nunwen, maiuscola, ascent, descent):
    """La scala dei glifi di nunwen: le maiuscole alte come quelle del carattere, se ci stanno nella riga."""
    o, h = nunwen['OS/2'], nunwen['hhea']
    return min(maiuscola / o.sCapHeight, (ascent + descent) / (h.ascent - h.descent))


def ttf_spleen(stame, misura, nunwen, percorso):
    glifi, w, h, ascent = spleen(stame, misura)
    U = 64                                               # unita' per pixel
    f = Ttf(h * U, ascent * U, (h - ascent) * U)

    def pixel(righe):
        def disegna(pen):
            for r, riga in enumerate(righe):
                y1 = (ascent - r) * U
                for m in re.finditer(r'#+', riga):        # una striscia per ogni corsa di pixel accesi
                    x0, x1 = m.start() * U, m.end() * U
                    pen.moveTo((x0, y1 - U)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y1 - U)); pen.closePath()
        return disegna

    for cp, righe in sorted(glifi.items()):
        f.aggiungi(cp, w * U, pixel(righe))
    for ch, gemello in GEMELLI.items():
        if ord(ch) not in f.cmap and ord(gemello) in glifi:
            f.aggiungi(ord(ch), w * U, pixel(glifi[ord(gemello)]))
    if 0x2014 not in f.cmap:                              # la lineetta: il trattino lungo tutta la cella
        meno = glifi[ord('-')]
        f.aggiungi(0x2014, w * U, pixel(['#' * w if '#' in riga else riga for riga in meno]))
    maiuscola = sum('#' in riga for riga in glifi[ord('H')]) * U
    f.riserva(nunwen, scala_riserva(nunwen, maiuscola, f.ascent, f.descent))
    f.salva(percorso, 'STAME Spleen %s' % misura,
            'Spleen: Copyright (c) 2018-2024, Frederic Cambus (BSD 2-Clause). Other glyphs: Nunito, '
            'Vernon Adams (SIL OFL 1.1), via nunwen.ttf of the SPRUCE theme.', maiuscola)
    return len(glifi)


P = 96          # passo di Matrice in unita': a 44 px un punto ogni 4 pixel, a 88 px ogni 8
RAGGIO = 0.42   # raggio del punto, in passi: il centro pieno e il bordo sfumato, come sulla plancia


def ttf_matrice(stame, nunwen, percorso):
    tab = matrice_completa(glifi_matrice(stame))
    # righe -2..-1 per gli accenti delle maiuscole, 0..6 la lettera, 7..8 il discendente
    f = Ttf(11 * P, 9 * P, 2 * P)
    k = P * RAGGIO / math.cos(math.pi / 8)

    def punti(insieme, dx):
        def disegna(pen):
            for r, c in sorted(insieme):
                cx, cy = (c + dx) * P + P / 2, (6 - r) * P + P / 2
                pen.qCurveTo(*[(round(cx + k * math.cos(-i * math.pi / 4)), round(cy + k * math.sin(-i * math.pi / 4)))
                               for i in range(8)], None)
                pen.closePath()
        return disegna

    for ch, (w, insieme) in sorted(tab.items()):
        dx = 0
        if ch.isdigit() and ch.isascii():                # cifre tutte larghe 5: l'orologio non balla
            dx, w = (5 - w) // 2, 5
        f.aggiungi(ord(ch), (w + 1) * P, punti(insieme, dx))
    f.riserva(nunwen, scala_riserva(nunwen, 7 * P, f.ascent, f.descent))
    f.salva(percorso, 'STAME Matrice', 'Matrice: Copyright (c) 2026 Luigi Massari (Stame, MIT). Other glyphs: '
            'Nunito, Vernon Adams (SIL OFL 1.1), via nunwen.ttf of the SPRUCE theme.', 7 * P)
    return len(tab)


# ---------------------------------------------------------------------------------------------------
# Immagini

def vetro(w, h, y0=0):
    """Il vetro della plancia: fondo e un punto della maglia ogni 4 pixel, a righe sfalsate."""
    img = np.empty((h, w, 3), np.float32)
    img[:] = rgbf('bg')
    ys = np.arange(y0, y0 + h)[:, None]
    xs = np.arange(w)[None, :]
    img[(ys % 4 == 0) & ((xs + np.where(ys % 8 == 0, 0, 2)) % 4 == 0)] = rgbf('mesh')
    return img


def rgba(img, alfa=255):
    a = alfa if np.ndim(alfa) else np.full(img.shape[:2] + (1,), alfa, np.float32)
    return np.dstack([img, a])


def pannello(w, h, filo):
    """Un pannello liscio con il filo di separazione in alto o in basso ('su', 'giu')."""
    img = np.empty((h, w, 3), np.float32)
    img[:] = rgbf('panel')
    img[0 if filo == 'su' else h - 1] = rgbf('line')
    return img


def cella_scelta(w, h, alfa):
    """La riga scelta: una cella scura e il segno del fuoco sul lato sinistro."""
    img = np.empty((h, w, 3), np.float32)
    img[:] = rgbf('bar')
    img[:, :max(4, w // 100)] = rgbf('focus')
    return rgba(img, alfa)


def inchiostro(im):
    """Icone delle console: un disegno a inchiostro come su un LCD (scuro dove l'immagine e' scura,
    carta dove e' chiara); i colori vivi prendono il colore del ruolo piu' vicino."""
    a = np.asarray(im.convert('RGBA'), np.float32)
    c = a[..., :3] / 255.0
    lum = (0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2])[..., None]
    out = rgbf('text') + (rgbf('kb_white') - rgbf('text')) * lum
    mx, mn = c.max(2), c.min(2)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    tinta = np.zeros(mx.shape, np.float32)
    d = np.maximum(mx - mn, 1e-6)
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    tinta = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    ruoli = [(0, 'warn'), (40, 'bass'), (60, 'active'), (120, 'focus'), (180, 'treble'), (250, 'loop'), (300, 'loop'), (360, 'warn')]
    gradi = np.array([g for g, _ in ruoli], np.float32)
    colori = np.array([rgb(n) for _, n in ruoli], np.float32)
    ruolo = colori[np.abs(tinta[..., None] - gradi).argmin(-1)]
    peso = (np.clip((sat - 0.35) / 0.3, 0, 1) * np.clip((mx - 0.25) / 0.3, 0, 1))[..., None]
    out = out * (1 - peso) + ruolo * peso
    return Image.fromarray(np.clip(np.round(np.dstack([out, a[..., 3:]])), 0, 255).astype(np.uint8), 'RGBA')


def marchio(stame):
    """Il marchio dei tre stami dall'avvio di Stame (avvio_3.png): copertura delle tre antere e dei
    filamenti con il ricettacolo, come immagini 'F' in alta risoluzione."""
    a = np.asarray(Image.open(os.path.join(stame, 'docs', 'identita', 'img', 'avvio_3.png')).convert('RGB'), np.float32)
    a = a[60:240, 230:410]
    fondo = np.median(a.reshape(-1, 3), axis=0)
    dist = np.abs(a - fondo).sum(2)
    tinta = a.reshape(-1, 3)[dist.argmax()]
    d = tinta - fondo
    cop = np.clip(((a - fondo) * d).sum(2) / (d * d).sum(), 0, 1)
    cop[cop < 0.12] = 0                                  # la maglia del vetro non e' marchio
    ys, xs = np.nonzero(cop)
    M = 12                                               # margine: np.roll non deve fare il giro dei bordi
    cop = np.pad(cop[ys.min():ys.max() + 1, xs.min():xs.max() + 1], M)
    pieno = cop > 0.5
    # le antere sono le parti spesse: si erode finche' i filamenti spariscono
    eroso = pieno.copy()
    for _ in range(5):
        eroso = eroso & np.roll(eroso, 1, 0) & np.roll(eroso, -1, 0) & np.roll(eroso, 1, 1) & np.roll(eroso, -1, 1)
    antere = []
    resto = eroso.copy()
    while resto.any():
        y, x = np.argwhere(resto)[0]
        blob = np.zeros_like(resto)
        blob[y, x] = True
        while True:
            cresce = blob | ((np.roll(blob, 1, 0) | np.roll(blob, -1, 0) | np.roll(blob, 1, 1) | np.roll(blob, -1, 1)) & resto)
            if (cresce == blob).all():
                break
            blob = cresce
        resto &= ~blob
        antere.append(blob)
    # le tre parti piu' grandi; le altre (dove i filamenti si uniscono) restano filamento
    antere = sorted(antere, key=lambda b: -b.sum())[:3]
    assert len(antere) == 3, 'marchio: %d antere trovate' % len(antere)
    for i, blob in enumerate(antere):
        for _ in range(8):
            blob = blob | np.roll(blob, 1, 0) | np.roll(blob, -1, 0) | np.roll(blob, 1, 1) | np.roll(blob, -1, 1)
        antere[i] = blob
    # dalla piu' bassa: le due laterali (sinistra, poi destra), poi quella centrale, che sta piu' in alto
    centrale = min(antere, key=lambda b: np.argwhere(b)[:, 0].min())
    laterali = sorted((b for b in antere if b is not centrale), key=lambda b: np.argwhere(b)[:, 1].mean())
    antere = laterali + [centrale]
    parti = [cop * ~(antere[0] | antere[1] | antere[2])] + [cop * b for b in antere]
    return [Image.fromarray(p[M:-M, M:-M].astype(np.float32), 'F') for p in parti]   # filamenti, antere


def disegna_marchio(parti, h, colori):
    """Il marchio alto h pixel: (colore, copertura) in float; colori: uno per parte (filamenti, antere)."""
    w0, h0 = parti[0].size
    w = max(1, int(round(w0 * h / h0)))
    cop = [np.clip(np.asarray(p.resize((w, h), Image.LANCZOS), np.float32), 0, 1) for p in parti]
    tot = np.clip(sum(cop), 0, 1)
    col = sum(c[..., None] * np.array(k, np.float32) for c, k in zip(cop, colori)) / np.maximum(sum(cop), 1e-6)[..., None]
    return col, tot


def barra_titolo(src, dst, parti):
    """bg-title.png: la testata, un pannello con il filo sotto, con il marchio dove spruce aveva l'albero."""
    a = np.asarray(Image.open(src).convert('RGBA'), np.float32)
    h, w = a.shape[:2]
    fondo = a[h // 2, -5, :3]
    diff = np.abs(a[:, :200, :3] - fondo).sum(axis=2) > 30
    ys, xs = np.nonzero(diff)
    out = rgba(pannello(w, h, 'giu'))
    if len(xs):
        x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
        col, cop = disegna_marchio(parti, y1 - y0 + 4, [rgb('treble')] * 4)
        tc.incolla(out, col, cop, (x0 + x1) // 2 - cop.shape[1] // 2, y0 - 2)
    tc.salva_rgba(out, dst)


def fotogrammi(dst_dir, prefisso, parti):
    """app_loading_0N / charging-0N: il marchio spento; poi si accendono i filamenti e le antere una
    alla volta, dalla piu' bassa, come all'avvio di Stame."""
    W, H, ph = 100, 480, 86
    acceso, spento = rgb('treble'), rgb('seg_off')
    for n in range(1, 6):
        colori = [acceso if n >= 2 else spento] + [acceso if n >= 3 + i else spento for i in range(3)]
        col, cop = disegna_marchio(parti, ph, colori)
        out = np.zeros((H, W, 4), np.float32)
        tc.incolla(out, col, cop, (W - cop.shape[1]) // 2, 240 - ph // 2)
        tc.salva_rgba(out, os.path.join(dst_dir, '%s%02d.png' % (prefisso, n)))


def salvataggio_vuoto(src, dst, parti):
    """save-default.png: anteprima di un salvataggio senza immagine, con il marchio al posto dell'albero."""
    w, h = Image.open(src).size
    out = np.zeros((h, w, 4), np.float32)
    col, cop = disegna_marchio(parti, 92, [rgb('ghost')] * 4)
    tc.incolla(out, col, cop, (w - cop.shape[1]) // 2, (h - cop.shape[0]) // 2)
    tc.salva_rgba(out, dst)


def squadre(img, colore, lato=14, margine=8):
    """Le squadre agli angoli, come su una tavola botanica."""
    h, w = img.shape[:2]
    for x, y, sx, sy in ((margine, margine, 1, 1), (w - 1 - margine, margine, -1, 1),
                         (margine, h - 1 - margine, 1, -1), (w - 1 - margine, h - 1 - margine, -1, -1)):
        xs = sorted((x, x + sx * lato))
        ys = sorted((y, y + sy * lato))
        img[y, xs[0]:xs[1] + 1] = colore
        img[ys[0]:ys[1] + 1, x] = colore


def salvaschermo(dst, parti, spleen_piccolo):
    """screensaver.png: il vetro con le squadre, «TAB. I» e il marchio; ora, data e batteria sono i widget."""
    img = vetro(640, 480)
    squadre(img, rgbf('ghost'))
    out = rgba(img)
    col, cop = disegna_marchio(parti, 120, [rgb('treble')] * 4)
    tc.incolla(out, col, cop, 320 - cop.shape[1] // 2, 44)
    im = Image.fromarray(np.clip(np.round(out), 0, 255).astype(np.uint8), 'RGBA')
    ImageDraw.Draw(im).text((20, 14), 'TAB. I', font=ImageFont.truetype(spleen_piccolo, 16), fill=rgb('dim'))
    im.save(dst, optimize=True)


# ---------------------------------------------------------------------------------------------------
# Suono, configurazione, crediti

def granello(dst):
    """sound/change.wav dal bounce di SoundCraft: mono, 150 ms con la coda sfumata, piano come il clic di Cirmolo."""
    with wave.open(os.path.join(RISORSE, 'granello.wav')) as w:
        sr, nc = w.getframerate(), w.getnchannels()
        a = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, nc).astype(np.float32).mean(1) / 32768
    n = int(sr * 0.15)
    a = a[:n] * np.concatenate([np.ones(n - int(sr * 0.04)), np.linspace(1, 0, int(sr * 0.04)) ** 2])
    a *= 10 ** (-22 / 20) / np.abs(a).max()             # picco a -22 dBFS
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with wave.open(dst, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(np.round(a * 32767).astype('<i2').tobytes())


def config(src, dst):
    with open(src, encoding='utf-8') as f:
        c = json.load(f)
    c['name'] = 'STAME'
    c['author'] = 'Cirmolo (dal tema Spruce di tenlevels, con i colori e i caratteri di Stame)'
    c['description'] = 'STAME theme for the Miyoo Flip: the reflective LCD of the Stame chord synth'
    c.pop('mainMenuTitle', None)        # vale quello di PyUI, «Cirmolo»
    testo, dim = GIORNO['text'], GIORNO['dim']
    c['batteryPercentage'] = {'color': dim, 'font': SPLEEN_GRANDE, 'size': 24}
    c['title'] = {'font': MATRICE_TTF, 'size': 44, 'color': testo}
    c['hint'] = {'font': SPLEEN_GRANDE, 'size': 24, 'color': dim}
    c['currentpage'] = {'font': SPLEEN_GRANDE, 'size': 24, 'color': dim}
    c['total'] = {'font': SPLEEN_GRANDE, 'size': 24, 'color': GIORNO['treble']}
    c['grid'] = {'font': SPLEEN_GRANDE, 'grid1x4': 24, 'grid3x4': 16, 'color': dim, 'selectedcolor': GIORNO['focus']}
    c['list'] = {'font': SPLEEN_GRANDE, 'size': 24, 'color': testo, 'selectedcolor': testo}
    # un carattere per uso (PyUI di Cirmolo, themes/theme.py): Matrice nella testata, Spleen piccolo
    # dove il testo e' piccolo; le misure sono quelle della cella, cosi' i pixel restano netti
    c['topBarFont'] = MATRICE_TTF
    c['topBarFontSize'] = 44
    c['gridMultiRowFont'] = SPLEEN_PICCOLO
    c['descriptionFont'] = SPLEEN_PICCOLO
    c['descriptionFontSize'] = 16
    # il nome del gioco scelto va nella banda in basso, non nella testata: la testata resta calma
    c['setTopBarTextToGameSelection'] = False
    c['screensaver'] = {
        'bgColor': GIORNO['bg'],
        'bgImage': 'screensaver.png',
        'overlayOpacity': 0,
        'screensaverTimeoutSec': 60,
        'widgets': [
            # i punti spenti: la cella piena di Matrice dietro ogni cifra, larga quanto l'ora
            {'type': 'text', 'value': '██:██', 'x': 0.5, 'y': 0.56, 'fontSize': 88, 'color': GIORNO['seg_off'],
             'font': MATRICE_TTF},
            {'type': 'clock', 'x': 0.5, 'y': 0.56, 'fontSize': 88, 'color': GIORNO['treble'], 'enabled': True,
             'font': MATRICE_TTF},
            {'type': 'date', 'x': 0.5, 'y': 0.76, 'fontSize': 24, 'color': GIORNO['dim'], 'font': SPLEEN_GRANDE},
            {'type': 'battery', 'x': 0.5, 'y': 0.86, 'fontSize': 16, 'color': GIORNO['ghost'], 'font': SPLEEN_PICCOLO},
        ],
    }
    with open(dst, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(c, f, ensure_ascii=False, indent=4)
        f.write('\n')


def icone_app():
    """Come in tema_cirmolo.py: le icone portate dalle app e quelle delle aree, ricolorate per STAME."""
    dst_dir = os.path.join(DESTINAZIONE, 'icons', 'app')
    n = 0
    for cfg in sorted(glob.glob(os.path.join(RADICE, 'App', '*', 'config.json'))):
        try:
            with open(cfg, encoding='utf-8') as f:
                nome = json.load(f).get('icon')
        except (OSError, ValueError):
            continue
        if not nome or os.path.basename(nome) == 'icon.png':
            continue
        nome = os.path.basename(nome)
        if os.path.exists(os.path.join(ORIGINE, 'icons', 'app', nome)):
            continue
        cartella = os.path.dirname(cfg)
        src = next((p for p in (os.path.join(cartella, nome), os.path.join(cartella, 'icon.png')) if os.path.isfile(p)), None)
        if src:
            tc.ricolora(Image.open(src), SEGMENTI).save(os.path.join(dst_dir, nome), optimize=True)
            n += 1
    for src in sorted(glob.glob(os.path.join(RADICE, 'App', 'PyUI', 'folder-icons', '*.png'))):
        tc.ricolora(Image.open(src), SEGMENTI).save(os.path.join(dst_dir, os.path.basename(src)), optimize=True)
        n += 1
    return n


CREDITI = """STAME theme
===========
The default theme of Cirmolo. Generated by .github/branding/tema_stame.py from the SPRUCE theme of
spruceOS (same layout and icons) with the resources of Stame (https://github.com/luigismith/stame,
MIT, (c) 2026 Luigi Massari): the colours of its Day theme, a reflective LCD; its mark; Matrice, its
dot-matrix typeface; and the "Stame grain" key sound, the E5 of its sound logo made with SoundCraft.

Fonts (see LICENSE-fonts.txt):
  stame-matrice.ttf        Matrice by Luigi Massari (Stame, MIT)
  stame-spleen-12x24.ttf   Spleen by Frederic Cambus, BSD 2-Clause
  stame-spleen-8x16.ttf    Spleen by Frederic Cambus, BSD 2-Clause
Each one also carries, for the characters it lacks, the glyphs of nunwen.ttf from the SPRUCE theme
(Nunito by Vernon Adams, SIL Open Font License 1.1), so the three fonts are released under the
SIL OFL 1.1 as a whole.

Credits of the original SPRUCE theme follow.

"""

OFL = """SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide development of collaborative font
projects, to support the font creation efforts of academic and linguistic communities, and to provide
a free and open framework in which fonts may be shared and improved in partnership with others.

The OFL allows the licensed fonts to be used, studied, modified and redistributed freely as long as
they are not sold by themselves. The fonts, including any derivative works, can be bundled, embedded,
redistributed and/or sold with any software provided that any reserved names are not used by
derivative works. The fonts and derivatives, however, cannot be released under any other type of
license. The requirement for fonts to remain under this license does not apply to any document
created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright Holder(s) under this license and
clearly marked as such. This may include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the copyright statement(s).

"Original Version" refers to the collection of Font Software components as distributed by the
Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting, or substituting -- in part
or in whole -- any of the components of the Original Version, by changing formats or by porting the
Font Software to a new environment.

"Author" refers to any designer, engineer, programmer, technical writer or other person who
contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining a copy of the Font Software, to
use, study, copy, merge, embed, modify, redistribute, and sell modified and unmodified copies of the
Font Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components, in Original or Modified Versions,
may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled, redistributed and/or sold with
any software, provided that each copy contains the above copyright notice and this license. These can
be included either as stand-alone text files, human-readable headers or in the appropriate
machine-readable metadata fields within text or binary files as long as those fields can be easily
viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font Name(s) unless explicit written
permission is granted by the corresponding Copyright Holder. This restriction only applies to the
primary font name as presented to the users.

4) The name(s) of the Copyright Holder(s) and the Author(s) of the Font Software shall not be used to
promote, endorse or advertise any Modified Version, except to acknowledge the contribution(s) of the
Copyright Holder(s) and the Author(s) or with their explicit written permission.

5) The Font Software, modified or unmodified, in part or in whole, must be distributed entirely under
this license, and must not be distributed under any other license. The requirement for fonts to
remain under this license does not apply to any document created using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE COPYRIGHT
HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, INCLUDING ANY GENERAL, SPECIAL, INDIRECT,
INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM OTHER DEALINGS IN THE FONT
SOFTWARE.
"""


def licenze(stame, dst):
    with open(os.path.join(stame, 'third_party', 'spleen', 'LICENSE'), encoding='utf-8') as f:
        spleen_bsd = f.read()
    testo = ('Fonts of the STAME theme of Cirmolo\n'
             '===================================\n\n'
             'stame-matrice.ttf: Matrice, Copyright (c) 2026 Luigi Massari (from Stame, MIT licence).\n'
             'stame-spleen-12x24.ttf, stame-spleen-8x16.ttf: Spleen, Copyright (c) 2018-2024, Frederic Cambus,\n'
             'BSD 2-Clause licence (below).\n'
             'All three also contain glyphs of nunwen.ttf (SPRUCE theme of spruceOS): Nunito by Vernon Adams,\n'
             'http://newtypography.co.uk, SIL Open Font License 1.1. "Nunito is a trademark of Vernon Adams."\n'
             'As modified versions that include those glyphs, the three fonts are distributed under the\n'
             'SIL Open Font License 1.1 (below).\n\n'
             '--- Spleen ---\n\n' + spleen_bsd + '\n\n--- SIL Open Font License 1.1 ---\n\n' + OFL)
    with open(dst, 'w', encoding='utf-8', newline='\n') as f:
        f.write(testo)


# ---------------------------------------------------------------------------------------------------

def genera(stame):
    if not os.path.isfile(os.path.join(stame, 'tools', 'matrice.py')):
        raise SystemExit('repository di Stame non trovato in %s: indicalo con --stame' % stame)
    if os.path.isdir(DESTINAZIONE):
        shutil.rmtree(DESTINAZIONE)
    os.makedirs(DESTINAZIONE)

    nunwen = TTFont(os.path.join(ORIGINE, 'nunwen.ttf'))
    n_m = ttf_matrice(stame, nunwen, os.path.join(DESTINAZIONE, MATRICE_TTF))
    n_g = ttf_spleen(stame, '12x24', nunwen, os.path.join(DESTINAZIONE, SPLEEN_GRANDE))
    n_p = ttf_spleen(stame, '8x16', nunwen, os.path.join(DESTINAZIONE, SPLEEN_PICCOLO))
    print('caratteri: Matrice %d glifi, Spleen 12x24 %d, 8x16 %d (piu\' la riserva di nunwen)' % (n_m, n_g, n_p))
    licenze(stame, os.path.join(DESTINAZIONE, 'LICENSE-fonts.txt'))

    granello(os.path.join(DESTINAZIONE, 'sound', 'change.wav'))
    with open(os.path.join(ORIGINE, 'CREDITS.txt'), encoding='utf-8', errors='replace') as f:
        crediti = f.read()
    with open(os.path.join(DESTINAZIONE, 'CREDITS.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(CREDITI + crediti)
    config(os.path.join(ORIGINE, 'config.json'), os.path.join(DESTINAZIONE, 'config.json'))

    n = 0
    for cartella in ('skin', 'icons'):
        for src in sorted(glob.glob(os.path.join(ORIGINE, cartella, '**', '*.png'), recursive=True)):
            rel = os.path.relpath(src, ORIGINE)
            dst = os.path.join(DESTINAZIONE, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            nome = os.path.basename(src)
            im = Image.open(src)
            alfa = np.asarray(im.convert('RGBA'), np.float32)[..., 3:4]
            h, w = alfa.shape[:2]
            if cartella == 'icons' and os.path.dirname(rel) == 'icons':
                inchiostro(im).save(dst, optimize=True)                  # le console
            elif tc.fondo_unito(im):
                y0 = tc.FASCE.get(nome, 0)
                img = vetro(w, h, y0)
                if nome == 'tips-bar-bg.png':
                    img[0] = rgbf('line')
                tc.salva_rgba(rgba(img, alfa), dst)
            elif nome in ('bg-list-s.png', 'bg-list-s2.png', 'bg-list-l.png'):
                tc.salva_rgba(cella_scelta(w, h, alfa), dst)
            elif nome == 'menu-sub-title-bg.png':
                tc.salva_rgba(rgba(pannello(w, h, 'giu'), alfa), dst)
            elif nome in SCELTA:
                tc.ricolora(im, SEGMENTI_SCELTA).save(dst, optimize=True)
            elif re.search(r'-f(-|\.png$)', nome):
                tc.ricolora(im, SEGMENTI_FUOCO).save(dst, optimize=True)
            else:
                tc.ricolora(im, SEGMENTI).save(dst, optimize=True)
            n += 1
        for altro in glob.glob(os.path.join(ORIGINE, cartella, '**', '*'), recursive=True):
            if os.path.isfile(altro) and not altro.endswith('.png'):
                dst = os.path.join(DESTINAZIONE, os.path.relpath(altro, ORIGINE))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(altro, dst)

    n += icone_app()

    parti = marchio(stame)
    skin = os.path.join(DESTINAZIONE, 'skin')
    barra_titolo(os.path.join(ORIGINE, 'skin', 'bg-title.png'), os.path.join(skin, 'bg-title.png'), parti)
    fotogrammi(skin, 'app_loading_', parti)
    fotogrammi(skin, 'charging-', parti)
    salvataggio_vuoto(os.path.join(ORIGINE, 'skin', 'save-default.png'), os.path.join(skin, 'save-default.png'), parti)
    salvaschermo(os.path.join(DESTINAZIONE, 'screensaver.png'), parti, os.path.join(DESTINAZIONE, SPLEEN_PICCOLO))
    anteprima_tema(os.path.join(DESTINAZIONE, 'preview.png'))
    print('tema STAME: %d immagini in %s' % (n, os.path.relpath(DESTINAZIONE, RADICE)))


def anteprima_tema(dst):
    """preview.png, per la scelta del tema: una schermata vera della Home presa dalla Flip
    (schermate/menu.png, la stessa del README) se c'e', altrimenti la Home ricostruita con le
    immagini del tema."""
    vera = os.path.join(QUI, 'schermate', 'menu.png')
    if os.path.isfile(vera):
        shutil.copy2(vera, dst)
        return
    t = lambda p: os.path.join(DESTINAZIONE, p)  # noqa: E731
    im = Image.open(t('skin/background.png')).convert('RGBA')
    im.alpha_composite(Image.open(t('skin/bg-title.png')).convert('RGBA'))
    d = ImageDraw.Draw(im)
    grande = ImageFont.truetype(t(SPLEEN_GRANDE), 24)
    titolo = ImageFont.truetype(t(MATRICE_TTF), 44)
    d.text((60, 30), '15:02', font=grande, fill=rgb('dim'), anchor='lm')
    d.text((320, 30), 'Cirmolo', font=titolo, fill=rgb('text'), anchor='mm')
    batt = Image.open(t('skin/ic-power-charge-100%.png')).convert('RGBA')
    im.alpha_composite(batt, (566 - batt.width, 30 - batt.height // 2))
    d.text((620, 30), '100', font=grande, fill=rgb('dim'), anchor='rm')
    voci = [('ic-music', 'Musica'), ('ic-app', 'App'), ('ic-favorite', 'Preferiti'), ('ic-game', 'Giochi'),
            ('ic-setting', 'Impostazioni')]
    for i, (icona, nome) in enumerate(voci):
        scelta = i == 0
        ic = Image.open(t('skin/%s-%s.png' % (icona, 'f' if scelta else 'n'))).convert('RGBA')
        cx = 72 + i * 124
        im.alpha_composite(ic, (cx - ic.width // 2, 170))
        d.text((cx, 293), nome, font=grande, fill=rgb('focus' if scelta else 'dim'), anchor='mm')
    im.convert('RGB').save(dst, optimize=True)


def anteprima(out):
    """Foglio di controllo: le immagini piu' visibili del tema, su fondo a scacchi grigio."""
    nomi = ['preview.png', 'screensaver.png', 'skin/bg-title.png', 'skin/bg-list-s.png', 'skin/ic-app-f.png',
            'skin/ic-app-n.png', 'skin/ic-music-f.png', 'skin/ic-setting-n.png',
            'skin/app_loading_01.png', 'skin/app_loading_02.png', 'skin/app_loading_03.png',
            'skin/app_loading_04.png', 'skin/app_loading_05.png', 'skin/save-default.png',
            'skin/icon-START.png', 'skin/icon-L2.png', 'skin/ic-power-charge-25%.png',
            'skin/ic-power-charge-100%.png', 'skin/list-item-select-bg-short.png', 'skin/bg-game-item-f.png',
            'icons/app/backup.png', 'icons/app/gallery.png', 'icons/gba.png', 'icons/psp.png', 'icons/n64.png',
            'icons/md.png', 'icons/gbc.png', 'icons/fc.png']
    foglio = Image.new('RGBA', (1400, 1300), (90, 90, 90, 255))
    x = y = riga = 0
    for nome in nomi:
        im = Image.open(os.path.join(DESTINAZIONE, nome)).convert('RGBA')
        if im.height == 480 and im.width == 100:
            im = im.crop((0, 180, 100, 300))
        if x + im.width > foglio.width:
            x, y, riga = 0, y + riga + 6, 0
        foglio.alpha_composite(im, (x, y))
        x += im.width + 6
        riga = max(riga, im.height)
    foglio.save(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--stame', default=STAME, help='cartella del repository di Stame (predefinita: ../stame)')
    ap.add_argument('--anteprima')
    args = ap.parse_args()
    genera(os.path.abspath(args.stame))
    if args.anteprima:
        anteprima(args.anteprima)


if __name__ == '__main__':
    main()
