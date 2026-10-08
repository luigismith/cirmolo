"""Test delle aree tematiche del menu App (App/PyUI/app-folders.json e menus/app/app_menu.py).

Uso (dalla radice del repo):
    python -I -X utf8 -m unittest discover -s .github/i18n/tests -v
"""
import ast
import json
import os
import unittest

RADICE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
PYUI = os.path.join(RADICE, 'App', 'PyUI')
CARTELLE = os.path.join(PYUI, 'app-folders.json')
MENU = os.path.join(PYUI, 'main-ui', 'menus', 'app', 'app_menu.py')


def carica():
    with open(CARTELLE, encoding='utf-8') as f:
        return json.load(f)


class PyUiAppConfig:                       # come apps/pyui_app.py, ridotto
    def __init__(self, label):
        self.label = label

    def get_label(self):
        return self.label


class AppCartella:
    def __init__(self, folder):
        self.folder = folder

    def get_folder(self):
        return self.folder


def funzioni_menu():
    """folder_key e folder_definitions di AppMenu, senza importare PyUI (servirebbe SDL)."""
    with open(MENU, encoding='utf-8') as f:
        albero = ast.parse(f.read())
    classe = next(n for n in albero.body if isinstance(n, ast.ClassDef) and n.name == 'AppMenu')
    metodi = [n for n in classe.body if isinstance(n, ast.FunctionDef) and n.name in ('folder_key', 'folder_definitions')]
    for m in metodi:
        m.decorator_list = []
    spazio = {'os': os, 'PyUiAppConfig': PyUiAppConfig}
    exec(compile(ast.Module(body=metodi, type_ignores=[]), MENU, 'exec'), spazio)
    return spazio['folder_key'], spazio['folder_definitions']


class TestCartelleApp(unittest.TestCase):

    def test_ogni_app_ha_un_area(self):
        dati = carica()
        chiavi = {c['key'] for c in dati['folders']}
        self.assertIn(dati['default'], chiavi)
        for area in list(dati['apps'].values()) + list(dati['pyui'].values()):
            self.assertIn(area, chiavi)
        senza_area = []
        app_dir = os.path.join(RADICE, 'App')
        for nome in sorted(os.listdir(app_dir)):
            cfg = os.path.join(app_dir, nome, 'config.json')
            if not os.path.isfile(cfg) or nome == 'PyUI':
                continue
            with open(cfg, encoding='utf-8') as f:
                c = json.load(f)
            dispositivi = c.get('devices')
            if dispositivi and 'MIYOO_FLIP' not in dispositivi:
                continue                    # sulla Flip non compare
            if nome not in dati['apps']:
                senza_area.append(nome)
        self.assertEqual(senza_area, [], 'aggiungile in App/PyUI/app-folders.json')

    def test_icone(self):
        for c in carica()['folders']:
            self.assertTrue(os.path.isfile(os.path.join(PYUI, 'folder-icons', c['icon'])), c['icon'])
        self.assertTrue(os.path.isfile(os.path.join(PYUI, 'folder-icons', 'altro.png')))

    def test_assegnazione(self):
        folder_key, folder_definitions = funzioni_menu()
        dati = carica()
        self.assertEqual(folder_key(AppCartella('/mnt/SDCARD/App/CirmoloSynth'), dati), 'music')
        self.assertEqual(folder_key(AppCartella('/mnt/SDCARD/App/RetroArch/'), dati), 'games')
        self.assertEqual(folder_key(AppCartella('/mnt/SDCARD/App/AppNuova'), dati), 'other')
        self.assertEqual(folder_key(PyUiAppConfig('Boxart Scraper'), dati), 'games')
        self.assertEqual(folder_key(None, dati), 'other')
        senza_altro = dict(dati, folders=[c for c in dati['folders'] if c['key'] != 'other'])
        self.assertEqual(folder_definitions(senza_altro)[-1]['key'], 'other')   # l'area predefinita c'e' sempre


if __name__ == '__main__':
    unittest.main()
