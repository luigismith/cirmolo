#!/usr/bin/env python3
"""Allinea App/PyUI/lang/English.json al codice e ai menu delle impostazioni, senza riformattare il file.

Aggiunge (solo se mancano), con il testo inglese identico al default usato dal codice:
  - "displayName" in cima;
  - le chiavi usate dal codice ma assenti, subito dopo la sezione fontPurposeSizes;
  - i nomi delle impostazioni mancanti in menuOptionDisplays;
  - le sezioni settingsCategories, menuOptionDescriptions, menuOptionValues e le altre sezioni
    mostrate a schermo, compresi i messaggi degli script (scriptMessages), inglese -> inglese.
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

    # 3) sezioni inglese -> inglese: voci mancanti in fondo alla sezione, oppure sezione nuova
    #    subito dopo la sezione precedente dell'elenco (la prima dopo menuOptionDisplays)
    def fine_sezione(nome):
        inizio = next((i for i, r in enumerate(righe) if r.startswith(IND + json.dumps(nome) + ':')), None)
        if inizio is None:
            return None
        return next(i for i in range(inizio, len(righe)) if righe[i].strip() in ('},', '}'))

    sezioni = dict(menu)
    sezioni.update(ek.stringhe_display())
    sezioni['scriptMessages'] = sorted(ek.stringhe_script())
    ordine = ['menuOptionDisplays', 'settingsCategories', 'menuOptionDescriptions', 'menuOptionValues',
              'appLabels', 'appDescriptions', 'taskLabels', 'taskDescriptions', 'pageTitles', 'appFolders', 'scriptMessages']
    for pos, sez in enumerate(ordine):
        esistenti = en.get(sez)
        voci = [v for v in sezioni.get(sez, []) if not (isinstance(esistenti, dict) and v in esistenti)]
        if isinstance(esistenti, dict):
            if not voci:
                continue
            i_fine = fine_sezione(sez)
            if not righe[i_fine - 1].rstrip().endswith(('{', ',')):
                righe[i_fine - 1] = righe[i_fine - 1].rstrip() + ','
            blocco = [riga(v, v, IND * 2) + ',' for v in voci]
            blocco[-1] = blocco[-1].rstrip(',')
            righe[i_fine:i_fine] = blocco
            aggiunte += ['%s: %s' % (sez, v) for v in voci]
        else:
            precedente = next((s for s in reversed(ordine[:pos]) if fine_sezione(s) is not None), None)
            i_dopo = fine_sezione(precedente)
            corpo = [riga(v, v, IND * 2) + ',' for v in voci]
            if corpo:
                corpo[-1] = corpo[-1].rstrip(',')
            nuova = ['%s%s: {' % (IND, json.dumps(sez))] + corpo + [IND + '},']
            if righe[i_dopo].strip() == '}':
                righe[i_dopo] = righe[i_dopo].rstrip() + ','
                nuova[-1] = nuova[-1].rstrip(',')
            righe[i_dopo + 1:i_dopo + 1] = nuova
            en[sez] = {}
            aggiunte.append('%s (sezione nuova, %d voci)' % (sez, len(voci)))

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
