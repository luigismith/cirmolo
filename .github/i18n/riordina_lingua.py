#!/usr/bin/env python3
"""Riscrive un file di lingua nello stesso ordine e formato di English.json, unendo eventuali traduzioni nuove.

- Le chiavi seguono l'ordine di English.json (anche dentro le sezioni); le chiavi che l'inglese non ha
  vengono scartate (con avviso), quelle mancanti NON vengono riempite (le segnala check_lang.py).
- Formato come English.json: 4 spazi, liste su una riga. Fine riga e codifica del file originale.
- Con --unisci FILE.json aggiunge/aggiorna traduzioni prese da un file (stessa struttura, anche parziale).

Uso:  python -I -X utf8 .github/i18n/riordina_lingua.py --lang Italian [--unisci nuove.json]
"""
import argparse
import json
import os
import sys

RADICE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
LANG = os.path.join(RADICE, 'App', 'PyUI', 'lang')
IND = '    '


def unisci(base, extra):
    for k, v in extra.items():
        if isinstance(v, dict) and isinstance(base.get(k), dict):
            unisci(base[k], v)
        else:
            base[k] = v


def serializza(en, tr, livello=1):
    righe = []
    chiavi = [k for k in en if k in tr]
    for i, k in enumerate(chiavi):
        virgola = ',' if i < len(chiavi) - 1 else ''
        v, ev = tr[k], en[k]
        pre = IND * livello + json.dumps(k, ensure_ascii=False) + ': '
        if isinstance(ev, dict) and isinstance(v, dict):
            righe.append(pre + '{')
            righe += serializza(ev, v, livello + 1)
            righe.append(IND * livello + '}' + virgola)
        elif isinstance(v, list):
            righe.append(pre + '[' + ', '.join(json.dumps(x, ensure_ascii=False) for x in v) + ']' + virgola)
        else:
            righe.append(pre + json.dumps(v, ensure_ascii=False) + virgola)
    return righe


def chiavi_extra(en, tr, percorso=''):
    out = []
    for k, v in tr.items():
        if k not in en:
            out.append(percorso + k)
        elif isinstance(v, dict) and isinstance(en[k], dict):
            out += chiavi_extra(en[k], v, percorso + k + '.')
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lang', default='Italian')
    ap.add_argument('--unisci')
    a = ap.parse_args()
    with open(os.path.join(LANG, 'English.json'), encoding='utf-8') as f:
        en = json.load(f)
    percorso = os.path.join(LANG, a.lang + '.json')
    grezzo = open(percorso, 'rb').read().decode('utf-8')
    eol = '\r\n' if '\r\n' in grezzo else '\n'
    tr = json.loads(grezzo)
    if a.unisci:
        with open(a.unisci, encoding='utf-8') as f:
            unisci(tr, json.load(f))
    for k in chiavi_extra(en, tr):
        print('avviso: «%s» non esiste in English.json: scartata' % k, file=sys.stderr)
    testo = '{' + eol + eol.join(serializza(en, tr)) + eol + '}' + eol
    json.loads(testo)
    with open(percorso, 'w', encoding='utf-8', newline='') as f:
        f.write(testo)
    print('%s.json riscritto nell\'ordine di English.json' % a.lang)


if __name__ == '__main__':
    main()
