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
APP = os.path.join(RADICE, 'App')
TASKS = os.path.join(RADICE, 'App', 'PyUI', 'spruce-tasks.json')

# Testi che gli script scrivono a runtime nei config.json delle app (runtimeHelper.sh, -OTA/downloader.sh)
# e app interne di PyUI: non stanno in nessun config.json, quindi li elenco qui.
APP_LABELS_EXTRA = ['Update Available', 'Check for Updates', 'Boxart Scraper', 'Activity Tracker']
APP_DESCRIPTIONS_EXTRA = ['Download and install updates over Wi-Fi', 'Version {version} is available',
                          'A new build of version {version} ({build}) is available']
# Titoli delle pagine di giochi (nomi passati a _run_rom_selection); i nomi dei sistemi non si traducono.
PAGE_TITLES = ['Favorites', 'Recents', 'Collections', 'Game List', 'Game Search', 'Game Switcher', '{system} Search']
# Valori mostrati con Language.menu_option_value() che non vengono da spruce-config.json
# (colori del salvaschermo in menus/settings/screensaver_settings_menu.py).
MENU_VALUES_EXTRA = ['White', 'Black', 'Dark gray', 'Gray', 'Red', 'Green', 'Blue', 'Yellow', 'Orange', 'Pink',
                     'Deep blue', 'Deep purple', 'Deep green', 'Deep red', 'Deep orange', 'Deep yellow', 'Deep teal']


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
    menu['menuOptionValues'].update(MENU_VALUES_EXTRA)
    return {k: sorted(v) for k, v in menu.items()}


def stringhe_display():
    """Testi mostrati cosi' come sono (etichette e descrizioni di app e strumenti, titoli di pagina):
    PyUI li traduce solo a schermo con Language.translate(sezione, testo_inglese)."""
    etichette, descrizioni = set(APP_LABELS_EXTRA), set(APP_DESCRIPTIONS_EXTRA)
    if os.path.isdir(APP):
        for nome in os.listdir(APP):
            cfg = os.path.join(APP, nome, 'config.json')
            if not os.path.isfile(cfg):
                continue
            try:
                with open(cfg, encoding='utf-8') as f:
                    d = json.load(f)
            except (ValueError, OSError):
                continue
            for chiave in ('label', '#label'):
                if isinstance(d.get(chiave), str) and d[chiave].strip():
                    etichette.add(d[chiave])
            if isinstance(d.get('description'), str) and d['description'].strip():
                descrizioni.add(d['description'])
    task_etichette, task_descrizioni = set(), set()
    if os.path.isfile(TASKS):
        with open(TASKS, encoding='utf-8') as f:
            t = json.load(f)
        for k, v in t.items():
            if k == 'descriptions':
                continue
            for parte in k.split('/'):
                if parte.strip():
                    task_etichette.add(parte)
        for v in (t.get('descriptions') or {}).values():
            if isinstance(v, str) and v.strip():
                task_descrizioni.add(v)
    return {'appLabels': sorted(etichette), 'appDescriptions': sorted(descrizioni),
            'taskLabels': sorted(task_etichette), 'taskDescriptions': sorted(task_descrizioni),
            'pageTitles': list(PAGE_TITLES)}


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
