#!/usr/bin/env python3
"""Allinea App/PyUI/lang/English.json al codice e ai menu delle impostazioni, senza riformattare il file.

Aggiunge (solo se mancano), con il testo inglese identico al default usato dal codice:
  - "displayName" in cima;
  - le chiavi usate dal codice ma assenti, subito dopo la sezione fontPurposeSizes;
  - i nomi delle impostazioni mancanti in menuOptionDisplays;
  - le sezioni settingsCategories, menuOptionDescriptions, menuOptionValues (inglese -> inglese).
Cosi' l'interfaccia inglese resta identica e ogni lingua puo' tradurre tutto.

Uso:  python -I -X utf8 .github/i18n/sync_english.py [--dry-run]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import estrai_chiavi as ek  # noqa: E402

PERCORSO = os.path.join(ek.LANG, 'English.json')
IND = '    '


def riga(k, v, ind=IND):
    return '%s%s: %s' % (ind, json.dumps(k, ensure_ascii=False), json.dumps(v, ensure_ascii=False))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    with open(PERCORSO, 'rb') as f:
        grezzo = f.read().decode('utf-8')
    eol = '\r\n' if '\r\n' in grezzo else '\n'   # conserva i fine riga del file originale
    testo = grezzo.replace('\r\n', '\n')
    en = json.loads(testo)
    codice = ek.chiavi_dal_codice()
    menu = ek.stringhe_menu()
    righe = testo.split('\n')
    aggiunte = []

    # 1) displayName in cima
    if 'displayName' not in en:
        righe.insert(1, riga('displayName', 'English') + ',')
        aggiunte.append('displayName')

    # 2) chiavi del codice mancanti, dopo la chiusura di fontPurposeSizes
    mancanti = [(k, v['default']) for k, v in sorted(codice.items()) if k not in en]
    if mancanti:
        i_fps = next(i for i, r in enumerate(righe) if r.strip().startswith('"fontPurposeSizes"'))
        i_fine = next(i for i in range(i_fps, len(righe)) if righe[i].strip() in ('},', '}'))
        blocco = [riga(k, v) + ',' for k, v in mancanti]
        righe[i_fine + 1:i_fine + 1] = blocco
        aggiunte += [k for k, _ in mancanti]

    # 3) menuOptionDisplays mancanti, in fondo alla sezione
    disp = en.get('menuOptionDisplays', {})
    nuovi_disp = [d for d in menu['menuOptionDisplays'] if d not in disp]
    i_mod = next(i for i, r in enumerate(righe) if r.strip().startswith('"menuOptionDisplays"'))
    i_mod_fine = next(i for i in range(i_mod, len(righe)) if righe[i].strip() in ('},', '}'))
    if nuovi_disp:
        if not righe[i_mod_fine - 1].rstrip().endswith(','):
            righe[i_mod_fine - 1] = righe[i_mod_fine - 1].rstrip() + ','
        blocco = [riga(d, d, IND * 2) + ',' for d in nuovi_disp]
        blocco[-1] = blocco[-1].rstrip(',')
        righe[i_mod_fine:i_mod_fine] = blocco
        i_mod_fine += len(blocco)
        aggiunte += ['menuOptionDisplays: ' + d for d in nuovi_disp]

    # 4) nuove sezioni subito dopo menuOptionDisplays
    nuove = []
    for sez in ('settingsCategories', 'menuOptionDescriptions', 'menuOptionValues'):
        if sez in en:
            continue
        voci = menu[sez]
        corpo = [riga(v, v, IND * 2) + ',' for v in voci]
        if corpo:
            corpo[-1] = corpo[-1].rstrip(',')
        nuove += ['%s%s: {' % (IND, json.dumps(sez))] + corpo + [IND + '},']
        aggiunte.append('%s (%d voci)' % (sez, len(voci)))
    if nuove:
        if righe[i_mod_fine].strip() == '}':
            righe[i_mod_fine] = righe[i_mod_fine].rstrip() + ','
            nuove[-1] = nuove[-1].rstrip(',')
        righe[i_mod_fine + 1:i_mod_fine + 1] = nuove

    nuovo = '\n'.join(righe)
    json.loads(nuovo)  # deve restare JSON valido
    for x in aggiunte:
        print('+ ' + x)
    print('%d aggiunte' % len(aggiunte))
    if not a.dry_run and aggiunte:
        with open(PERCORSO, 'w', encoding='utf-8', newline='') as f:
            f.write(nuovo.replace('\n', eol))


if __name__ == '__main__':
    main()
