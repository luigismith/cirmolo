#!/usr/bin/env python3
"""Estrae tutte le stringhe traducibili dell'interfaccia PyUI e dei menu delle impostazioni.

Uscita (JSON): per ogni chiave il testo inglese predefinito usato dal codice e i file in cui compare,
piu' le stringhe dei menu di spruce-config.json (nomi, descrizioni, valori, categorie).
Serve per tenere English.json allineato al codice e per sapere cosa tradurre.

Uso:  python -I -X utf8 .github/i18n/estrai_chiavi.py [--out chiavi.json]
"""
import argparse
import ast
import json
import os
import re
import sys

RADICE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
MAIN_UI = os.path.join(RADICE, 'App', 'PyUI', 'main-ui')
LANG = os.path.join(RADICE, 'App', 'PyUI', 'lang')
SPRUCE_CONFIG = os.path.join(RADICE, 'Saves', 'spruce', 'spruce-config.json')
EMU = os.path.join(RADICE, 'Emu')


def stringa(nodo):
    return nodo.value if isinstance(nodo, ast.Constant) and isinstance(nodo.value, str) else None


def chiavi_dal_codice():
    trovate = {}
    for cartella, _, files in os.walk(MAIN_UI):
        for nome in files:
            if not nome.endswith('.py'):
                continue
            percorso = os.path.join(cartella, nome)
            rel = os.path.relpath(percorso, RADICE).replace('\\', '/')
            with open(percorso, encoding='utf-8') as f:
                try:
                    albero = ast.parse(f.read())
                except SyntaxError:
                    continue
            for n in ast.walk(albero):
                if not isinstance(n, ast.Call) or not isinstance(n.func, ast.Attribute) or len(n.args) < 2:
                    continue
                f_nome = n.func.attr
                oggetto = n.func.value
                chi = getattr(oggetto, 'id', None) or getattr(oggetto, 'attr', None)
                in_language = rel.endswith('menus/language/language.py')
                ok = (chi == 'Language' and f_nome in ('label', 'get')) or \
                     (in_language and chi == 'cls' and f_nome == 'label') or \
                     (in_language and chi == '_data' and f_nome == 'get' and
                      isinstance(oggetto, ast.Attribute) and getattr(oggetto.value, 'id', None) == 'cls')
                if not ok:
                    continue
                k, d = stringa(n.args[0]), stringa(n.args[1])
                if k is None or d is None:
                    continue
                voce = trovate.setdefault(k, {'default': d, 'files': []})
                if rel not in voce['files']:
                    voce['files'].append(rel)
                if voce['default'] != d:
                    voce.setdefault('altri_default', []).append(d)
    return trovate


def stringhe_menu():
    menu = {'menuOptionDisplays': set(), 'menuOptionDescriptions': set(), 'menuOptionValues': set(), 'settingsCategories': set()}
    if os.path.exists(SPRUCE_CONFIG):
        with open(SPRUCE_CONFIG, encoding='utf-8') as f:
            conf = json.load(f)
        for categoria, opzioni in conf.get('menuOptions', {}).items():
            menu['settingsCategories'].add(categoria)
            for opz in opzioni.values():
                if not isinstance(opz, dict):
                    continue
                if opz.get('display'):
                    menu['menuOptionDisplays'].add(opz['display'])
                if opz.get('description'):
                    menu['menuOptionDescriptions'].add(opz['description'])
                for v in opz.get('options', []) or []:
                    if isinstance(v, str) and re.search('[A-Za-z]{2}', v) and v not in ('True', 'False'):
                        menu['menuOptionValues'].add(v)
    return {k: sorted(v) for k, v in menu.items()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out')
    a = ap.parse_args()
    codice = chiavi_dal_codice()
    with open(os.path.join(LANG, 'English.json'), encoding='utf-8') as f:
        en = json.load(f)
    mancanti = {k: v for k, v in codice.items() if k not in en}
    rep = {'chiavi_codice': codice, 'mancanti_in_english': mancanti, 'menu': stringhe_menu()}
    testo = json.dumps(rep, ensure_ascii=False, indent=1)
    if a.out:
        with open(a.out, 'w', encoding='utf-8') as f:
            f.write(testo)
    print('chiavi usate dal codice: %d, mancanti in English.json: %d' % (len(codice), len(mancanti)), file=sys.stderr)
    for k, v in sorted(mancanti.items()):
        print('  %-40s %s' % (k, v['default']), file=sys.stderr)
    for sez, valori in rep['menu'].items():
        assenti = [x for x in valori if x not in (en.get(sez) or {})]
        print('%s: %d nel config, %d assenti in English.json' % (sez, len(valori), len(assenti)), file=sys.stderr)


if __name__ == '__main__':
    main()
