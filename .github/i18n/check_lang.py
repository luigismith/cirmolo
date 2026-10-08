#!/usr/bin/env python3
"""Controlla un file di lingua di PyUI rispetto a English.json.

Errori (fanno fallire il controllo):
  - JSON non valido, BOM, chiavi duplicate
  - chiavi o struttura diverse dall'inglese (sezioni, liste della stessa lunghezza)
  - stringhe vuote dove l'inglese non lo e', spazi iniziali/finali diversi dall'inglese
  - segnaposto diversi dall'inglese: {nome}, $system
  - stringhe usate con str.format (date del salvaschermo, descrizioni dei menu) che non si
    formattano o hanno graffe in piu': un errore qui manda in crash la schermata
Avvisi (falliscono solo con --strict):
  - testo identico all'inglese (esclusi nomi propri e sigle in una lista di eccezioni)
  - testo molto piu' lungo dell'inglese
  - errori di stile italiani tipici (E' al posto di È, pò, qual'è, un'altro, doppi spazi)

Uso:  python -I -X utf8 .github/i18n/check_lang.py --lang Italian [--strict]
"""
import argparse
import json
import os
import re
import string
import sys

RADICE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
LANG = os.path.join(RADICE, 'App', 'PyUI', 'lang')

SEGNAPOSTO = re.compile(r'\{[A-Za-z_][A-Za-z0-9_]*\}|\$system')
# chiavi (o sezioni) i cui testi passano da str.format
FORMAT_CHIAVI = {'screensaverDateFormat'}
FORMAT_SEZIONI = {'menuOptionDescriptions'}
# testi che possono restare uguali all'inglese
UGUALI_OK = {
    'A', 'B', 'X', 'Y', 'L1', 'L2', 'L3', 'R1', 'R2', 'R3', 'Start', 'Select', 'Menu', 'No', 'OK',
    'Volume', 'Bluetooth', 'Password', 'Screenshot', 'Game Switcher', 'RetroArch', 'RetroAchievements',
    'PPSSPP', 'Casual', 'Hardcore', 'Nightly', 'Standard', 'Audio', 'Proxy', 'LED', 'Overclock',
    '128MB', '256MB', '512MB', 'Apotris (GBA)', 'L2+R2+DOWN', 'Menu + Vol', 'Songo#5', 'Splore',
    'Start + L/R', 'http', 'mono', 'socks5', 'spruce', 'spruceUI', 'Mostra AM/PM',
    'http://{ip_addr}:8080', 'http://{ip_addr}:8384', 'ssh spruce@{ip_addr} (password=happygaming)',
}
STILE = [
    (re.compile(r"\bE'\s"), "«E'» invece di «È»"),
    (re.compile(r'\bpò\b'), "«pò» invece di «po'»"),
    (re.compile(r"\bqual'è\b", re.I), "«qual'è» invece di «qual è»"),
    (re.compile(r"\bun'altro\b", re.I), "«un'altro» invece di «un altro»"),
    (re.compile(r'  '), 'doppio spazio'),
]


def carica(percorso):
    with open(percorso, 'rb') as f:
        dati = f.read()
    if dati.startswith(b'\xef\xbb\xbf'):
        raise ValueError('il file inizia con un BOM UTF-8')
    duplicate = []

    def senza_duplicati(coppie):
        visto = {}
        for k, v in coppie:
            if k in visto:
                duplicate.append(k)
            visto[k] = v
        return visto
    obj = json.loads(dati.decode('utf-8'), object_pairs_hook=senza_duplicati)
    return obj, duplicate


def campi_format(testo):
    return {campo for _, campo, _, _ in string.Formatter().parse(testo) if campo is not None}


def confronta(en, tr, percorso, errori, avvisi, sezione=None):
    if isinstance(en, dict):
        if not isinstance(tr, dict):
            errori.append('%s: deve essere una sezione (oggetto)' % percorso)
            return
        for k in en:
            if k not in tr:
                errori.append('%s: manca la chiave «%s»' % (percorso, k))
        for k in tr:
            if k not in en:
                errori.append('%s: chiave «%s» non presente in English.json' % (percorso, k))
        for k in en:
            if k in tr:
                confronta(en[k], tr[k], '%s.%s' % (percorso, k) if percorso else k, errori, avvisi,
                          sezione=k if percorso == '' else sezione)
        return
    if isinstance(en, list):
        if not isinstance(tr, list) or len(tr) != len(en):
            errori.append('%s: deve essere una lista di %d elementi' % (percorso, len(en)))
            return
        for i, (a, b) in enumerate(zip(en, tr)):
            confronta(a, b, '%s[%d]' % (percorso, i), errori, avvisi, sezione)
        return
    if not isinstance(en, str):
        if en != tr:
            errori.append('%s: valore non testuale diverso dall\'inglese' % percorso)
        return
    if not isinstance(tr, str):
        errori.append('%s: deve essere un testo' % percorso)
        return
    if en and not tr:
        errori.append('%s: testo vuoto' % percorso)
        return
    if (en[:1] == ' ') != (tr[:1] == ' ') or (en[-1:] == ' ') != (tr[-1:] == ' '):
        errori.append('%s: spazi iniziali/finali diversi dall\'inglese' % percorso)
    if sorted(SEGNAPOSTO.findall(en)) != sorted(SEGNAPOSTO.findall(tr)):
        errori.append('%s: segnaposto diversi (en %s, tr %s)' % (percorso, SEGNAPOSTO.findall(en), SEGNAPOSTO.findall(tr)))
    chiave = percorso.split('.')[-1]
    if chiave in FORMAT_CHIAVI or sezione in FORMAT_SEZIONI:
        try:
            campi_tr = campi_format(tr)
            campi_en = campi_format(en)
            if not campi_tr <= campi_en:
                errori.append('%s: campi str.format non presenti in inglese: %s' % (percorso, sorted(campi_tr - campi_en)))
            prova = {c: 'x' for c in campi_en}
            tr.format(**prova)
        except (ValueError, KeyError, IndexError) as e:
            errori.append('%s: non si formatta con str.format (%s)' % (percorso, e))
    if tr == en and en.strip() and tr not in UGUALI_OK and re.search('[A-Za-z]{3}', en):
        avvisi.append('%s: uguale all\'inglese: «%s»' % (percorso, en))
    if len(en) >= 12 and len(tr) > len(en) * 1.6 and len(tr) - len(en) > 12:
        avvisi.append('%s: molto piu\' lungo dell\'inglese (%d contro %d)' % (percorso, len(tr), len(en)))
    for rx, msg in STILE:
        if rx.search(tr):
            avvisi.append('%s: stile: %s' % (percorso, msg))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lang', default='Italian')
    ap.add_argument('--strict', action='store_true')
    a = ap.parse_args()
    errori, avvisi = [], []
    en, dup_en = carica(os.path.join(LANG, 'English.json'))
    tr, dup_tr = carica(os.path.join(LANG, a.lang + '.json'))
    errori += ['English.json: chiave duplicata «%s»' % k for k in dup_en]
    errori += ['%s.json: chiave duplicata «%s»' % (a.lang, k) for k in dup_tr]
    confronta(en, tr, '', errori, avvisi)
    testi = []

    def raccogli(o):
        if isinstance(o, dict):
            for v in o.values():
                raccogli(v)
        elif isinstance(o, list):
            for v in o:
                raccogli(v)
        elif isinstance(o, str):
            testi.append(o)
    raccogli(tr)
    for e in errori:
        print('ERRORE  ' + e)
    for w in avvisi:
        print('avviso  ' + w)
    print('%s.json: %d testi, %d errori, %d avvisi' % (a.lang, len(testi), len(errori), len(avvisi)))
    if errori or (a.strict and avvisi):
        sys.exit(1)


if __name__ == '__main__':
    main()
