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
    cartelle = {'Other'}                  # area predefinita (menus/app/app_menu.py)
    af = os.path.join(RADICE, 'App', 'PyUI', 'app-folders.json')
    if os.path.isfile(af):
        with open(af, encoding='utf-8') as f:
            for c in json.load(f).get('folders', []):
                if isinstance(c, dict) and c.get('label'):
                    cartelle.add(c['label'])
    return {'appLabels': sorted(etichette), 'appDescriptions': sorted(descrizioni),
            'taskLabels': sorted(task_etichette), 'taskDescriptions': sorted(task_descrizioni),
            'pageTitles': list(PAGE_TITLES), 'appFolders': sorted(cartelle)}


# --- Messaggi degli script shell -------------------------------------------------------------
# Gli script mostrano testi con display -t, log_and_display_message, display_image_and_text e
# display_text_with_percentage_bar; spruce/scripts/translate_message.jq li traduce a schermo
# cercando il testo inglese nella sezione scriptMessages. Le parti variabili ($VAR, ${VAR},
# $(...)) diventano segnaposto {nome}.
CARTELLE_SCRIPT = [os.path.join(RADICE, 'spruce', 'scripts'), APP]
ESCLUSI_SCRIPT = ('/App/PortMaster/', '/legacy_display.sh', '/platform/device.sh')
CHIAMATE = ('log_and_display_message', 'display_image_and_text', 'display_text_with_percentage_bar', 'display',
            'flip_abort')
_RX_CHIAMATA = re.compile(r'(?<![\w$/.-])(%s)(?=[ \t])' % '|'.join(CHIAMATE))
_PRIMA_OK = ('then', 'do', 'else', 'if', 'elif', '!', 'while', 'until')


def _fine_espansione(s, i):
    """s[i] == '$': indice dopo l'espansione e il nome del segnaposto."""
    if s.startswith('$((', i) or s.startswith('$(', i):
        prof, j = 0, i + 1
        while j < len(s):
            if s[j] == '(':
                prof += 1
            elif s[j] == ')':
                prof -= 1
                if prof == 0:
                    return j + 1, 'value'
            j += 1
        return len(s), 'value'
    if s.startswith('${', i):
        j = s.find('}', i)
        j = len(s) - 1 if j < 0 else j
        nome = re.match(r'[#!]?([A-Za-z_][A-Za-z0-9_]*|[0-9]+)', s[i + 2:j])
        return j + 1, (nome.group(1) if nome else 'value')
    m = re.match(r'\$([A-Za-z_][A-Za-z0-9_]*|[0-9?#@*$!-])', s[i:])
    if m:
        return i + len(m.group(0)), m.group(1)
    return i + 1, None


def _parole(s, i):
    """Parole di un comando shell a partire da s[i], fino a fine riga/; | & non quotati.
    Ogni parola e' una lista di pezzi ('lett', testo) o ('var', nome)."""
    parole, pezzi, j, in_parola = [], [], i, False

    def chiudi():
        nonlocal pezzi, in_parola
        if in_parola:
            uniti = []
            for tipo, val in pezzi:   # unisce i pezzi di testo consecutivi
                if tipo == 'lett' and uniti and uniti[-1][0] == 'lett':
                    uniti[-1] = ('lett', uniti[-1][1] + val)
                else:
                    uniti.append((tipo, val))
            parole.append(uniti)
        pezzi, in_parola = [], False

    while j < len(s):
        c = s[j]
        if c == '\\' and s.startswith('\\\n', j):
            j += 2
            continue
        if c in ' \t':
            chiudi(); j += 1; continue
        if c in '\n;|&)' or (c == '#' and not in_parola):
            break
        in_parola = True
        if c == "'":
            k = s.find("'", j + 1)
            k = len(s) if k < 0 else k
            pezzi.append(('lett', s[j + 1:k])); j = k + 1
        elif c == '"':
            j += 1
            while j < len(s) and s[j] != '"':
                if s[j] == '\\' and j + 1 < len(s) and s[j + 1] in '$`"\\\n':
                    if s[j + 1] != '\n':
                        pezzi.append(('lett', s[j + 1]))
                    j += 2
                elif s[j] == '$':
                    k, nome = _fine_espansione(s, j)
                    pezzi.append(('var', nome) if nome else ('lett', '$')); j = k
                elif s[j] == '`':
                    k = s.find('`', j + 1)
                    pezzi.append(('var', 'value')); j = (len(s) if k < 0 else k) + 1
                else:
                    pezzi.append(('lett', s[j])); j += 1
            j += 1
        elif c == '$':
            k, nome = _fine_espansione(s, j)
            pezzi.append(('var', nome) if nome else ('lett', '$')); j = k
        else:
            pezzi.append(('lett', '\\' + s[j + 1] if c == '\\' and j + 1 < len(s) else c))
            j += 2 if c == '\\' else 1
    chiudi()
    return parole


def _modello(pezzi):
    """Testo inglese con {segnaposto}; None se e' tutto variabile o contiene graffe vere."""
    out, usati, lettere = [], {}, ''
    for tipo, val in pezzi:
        if tipo == 'lett':
            if '{' in val or '}' in val:
                return None
            out.append(val); lettere += val
        else:
            nome = re.sub(r'[^a-z0-9_]', '', val.lower()) or 'value'
            if nome[0].isdigit() or nome in ('', '_'):
                nome = 'arg' + nome
            usati[nome] = usati.get(nome, 0) + 1
            out.append('{%s}' % (nome if usati[nome] == 1 else '%s%d' % (nome, usati[nome])))
    if not re.search('[A-Za-z]{2}', lettere):
        return None
    return ''.join(out)


UPDATER = os.path.join(APP, '-Updater', 'updater.py')


def _modello_py(nodo):
    """Stringa o f-string Python -> testo con {segnaposto}; None se non e' un testo fisso."""
    if isinstance(nodo, ast.Constant) and isinstance(nodo.value, str):
        pezzi = [('lett', nodo.value)]
    elif isinstance(nodo, ast.JoinedStr):
        pezzi = []
        for v in nodo.values:
            if isinstance(v, ast.Constant):
                pezzi.append(('lett', v.value))
            else:
                e = v.value
                if isinstance(e, ast.Subscript) and isinstance(e.slice, ast.Constant) and isinstance(e.slice.value, str):
                    nome = e.slice.value
                elif isinstance(e, ast.Attribute):
                    nome = e.attr
                elif isinstance(e, ast.Name):
                    nome = e.id
                else:
                    nome = 'value'
                pezzi.append(('var', nome))
    else:
        return None
    return _modello(pezzi)


def stringhe_updater():
    """Messaggi che App/-Updater/updater.py manda a PyUI (ui.image_and_text, ui.progress_bar, fail)."""
    trovate = {}
    if not os.path.isfile(UPDATER):
        return trovate
    rel = os.path.relpath(UPDATER, RADICE).replace('\\', '/')
    with open(UPDATER, encoding='utf-8') as f:
        albero = ast.parse(f.read())
    for n in ast.walk(albero):
        if not isinstance(n, ast.Call):
            continue
        nome = n.func.attr if isinstance(n.func, ast.Attribute) else getattr(n.func, 'id', None)
        kw = {k.arg: k.value for k in n.keywords}
        if nome == 'image_and_text':
            testi = [n.args[3] if len(n.args) > 3 else kw.get('text')]
        elif nome == 'progress_bar':
            testi = [n.args[0] if n.args else kw.get('text'), n.args[2] if len(n.args) > 2 else kw.get('bottom')]
        elif nome == 'fail' and isinstance(n.func, ast.Name):
            testi = [n.args[0] if n.args else kw.get('msg')]
        else:
            continue
        for t in testi:
            m = _modello_py(t) if t is not None else None
            if m:
                trovate.setdefault(m, [])
                if rel not in trovate[m]:
                    trovate[m].append(rel)
    return trovate


def stringhe_script():
    """{testo inglese: [file]} per i messaggi che gli script mostrano a schermo."""
    trovate = stringhe_updater()
    for base in CARTELLE_SCRIPT:
        for cartella, _, files in os.walk(base):
            for nome in files:
                if not nome.endswith('.sh'):
                    continue
                percorso = os.path.join(cartella, nome)
                rel = os.path.relpath(percorso, RADICE).replace('\\', '/')
                if any(x in '/' + rel for x in ESCLUSI_SCRIPT):
                    continue
                with open(percorso, encoding='utf-8', errors='replace') as f:
                    s = f.read()
                for m in _RX_CHIAMATA.finditer(s):
                    riga = s[s.rfind('\n', 0, m.start()) + 1:m.start()]
                    prima = riga.strip()
                    if prima.startswith('#') or re.match(r'\s*%s\s*\(\)' % m.group(1), s[m.start():m.start() + 60]):
                        continue
                    if prima and not (prima[-1] in ';|&({' or prima.split()[-1] in _PRIMA_OK):
                        continue
                    arg = _parole(s, m.end())
                    testi = []
                    if m.group(1) == 'display':
                        testi = [arg[k + 1] for k, p in enumerate(arg[:-1]) if p in ([('lett', '-t')], [('lett', '--text')])]
                    elif m.group(1) == 'log_and_display_message':
                        testi = arg[:1]
                    elif m.group(1) == 'display_text_with_percentage_bar':
                        testi = arg[:1] + arg[2:3]
                    elif m.group(1) == 'flip_abort':          # App/BootLogo/install_logo.sh: $2 va a schermo
                        testi = arg[1:2]
                    elif m.group(1) == 'display_image_and_text':
                        testi = arg[1:2] if len(arg) == 2 else arg[3:4]
                    for p in testi:
                        t = _modello(p)
                        if t:
                            trovate.setdefault(t, [])
                            if rel not in trovate[t]:
                                trovate[t].append(rel)
    return trovate


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out')
    ap.add_argument('--script', action='store_true', help='elenca i messaggi degli script shell')
    a = ap.parse_args()
    if a.script:
        for t, files in sorted(stringhe_script().items()):
            print('%-90s %s' % (json.dumps(t, ensure_ascii=False), ', '.join(files)))
        return
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
