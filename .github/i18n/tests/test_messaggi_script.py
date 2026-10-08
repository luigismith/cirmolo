"""Test della traduzione dei messaggi degli script (spruce/scripts/translate_message.jq).

Il filtro jq deve dare lo stesso risultato di Language.translate('scriptMessages', ...) di PyUI,
per ogni voce di Italian.json (con valori di prova al posto dei segnaposto) e nei casi limite.
Serve jq 1.7 (sulla console c'e' spruce/bin64/jq): variabile JQ, oppure jq nel PATH.

Uso (dalla radice del repo):
    JQ=/percorso/jq python -I -X utf8 -m unittest discover -s .github/i18n/tests -v
"""
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

RADICE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
MAIN_UI = os.path.join(RADICE, 'App', 'PyUI', 'main-ui')
LANG = os.path.join(RADICE, 'App', 'PyUI', 'lang')
FILTRO = os.path.join(RADICE, 'spruce', 'scripts', 'translate_message.jq')
sys.path.insert(0, MAIN_UI)

from utils.logger import PyUiLogger  # noqa: E402

PyUiLogger._logger = logging.getLogger('i18n-test')
from menus.language.language import Language  # noqa: E402

sys.path.insert(0, os.path.join(RADICE, '.github', 'i18n'))
import estrai_chiavi  # noqa: E402

JQ = os.environ.get('JQ') or shutil.which('jq')


def programma_in_blocco():
    """Il filtro applicato a un elenco di testi in un solo processo: stessa logica, $t da $T."""
    with open(FILTRO, encoding='utf-8') as f:
        righe = [r for r in f.read().split('\n') if not r.lstrip().startswith('#')]
    testo = '\n'.join(righe)
    i = testo.index('(.scriptMessages')
    return testo[:i] + '$L[0] as $lang | [$T[0][] as $t | $lang | (' + testo[i:] + ')]'


def jq_traduci(file_lingua, testi):
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, 'testi.json')
        with open(p, 'w', encoding='utf-8') as f:
            json.dump(testi, f, ensure_ascii=False)
        prog = os.path.join(d, 'prog.jq')
        with open(prog, 'w', encoding='utf-8') as f:
            f.write(programma_in_blocco())
        out = subprocess.run([JQ, '-n', '-c', '--slurpfile', 'L', file_lingua, '--slurpfile', 'T', p, '-f', prog],
                             capture_output=True, check=True).stdout.decode('utf-8')
    return json.loads(out)


def jq_singolo(file_lingua, testo):
    """Come lo chiama translate_message() sulla console (un testo, --arg t), ma con --rawfile:
    su Windows la riga di comando altererebbe le barre rovesciate."""
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, 't.txt')
        with open(p, 'w', encoding='utf-8', newline='') as f:
            f.write(testo)
        out = subprocess.run([JQ, '-j', '--rawfile', 't', p, '-f', FILTRO, file_lingua],
                             capture_output=True, check=True).stdout
    out = out.decode('utf-8')
    return out.replace('\r\n', '\n') if os.name == 'nt' else out   # jq per Windows scrive CRLF


def py_traduci(file_lingua, testi):
    Language._data = {}
    Language._read_from_file(file_lingua)
    return [Language.translate('scriptMessages', t) for t in testi]


def istanze(chiave, n=0):
    """La voce con valori di prova al posto dei segnaposto."""
    valori = ['Alfa', '42', 'Beta Gamma', '7.5', 'x/y']
    i = iter(range(100))
    return re.sub(r'\{[A-Za-z_][A-Za-z0-9_]*\}', lambda m: valori[(next(i) + n) % len(valori)], chiave)


@unittest.skipUnless(JQ, 'jq non trovato: imposta JQ o mettilo nel PATH')
class TestMessaggiScript(unittest.TestCase):

    def test_italiano_jq_uguale_a_pyui(self):
        f = os.path.join(LANG, 'Italian.json')
        with open(f, encoding='utf-8') as fp:
            voci = json.load(fp)['scriptMessages']
        testi = list(voci) + [istanze(k, n) for k in voci if '{' in k for n in (0, 2)]
        testi += ['Messaggio sconosciuto', '', 'Checking for updates... extra']
        attesi = py_traduci(f, testi)
        ottenuti = jq_traduci(f, testi)
        diversi = [(t, a, o) for t, a, o in zip(testi, attesi, ottenuti) if a != o]
        self.assertEqual(diversi, [])
        # e le voci esatte tornano proprio l'italiano
        for k, v in voci.items():
            self.assertEqual(ottenuti[testi.index(k)], v)

    def test_inglese_lascia_tutto_com_e(self):
        f = os.path.join(LANG, 'English.json')
        with open(f, encoding='utf-8') as fp:
            voci = json.load(fp)['scriptMessages']
        testi = [istanze(k) for k in voci]
        self.assertEqual(jq_traduci(f, testi), testi)

    def test_casi_limite(self):
        tabella = {'scriptMessages': {
            'Unpacking {section_label}\\n{detail_line}': 'Estrazione: {section_label}\\n{detail_line}',
            'Sprucing up...\\nUnpacking {section_label}\\n{detail_line}': 'Preparo...\\nEstrazione: {section_label}\\n{detail_line}',
            'Path C:\\temp {x} (a+b)*[c]?': 'Percorso C:\\temp {x} (a+b)*[c]?',
            'Sync stalled\nNo progress for {s}s': 'Bloccata\nFermo da {s} s',
            '{name} installed!': '{name} installato!',
            'Themes': 'Temi',
            'Price $5 ^ok': 'Prezzo $5 ^ok',
        }}
        casi = {
            'Unpacking Themes\\nfile 3 of 9': 'Estrazione: Temi\\nfile 3 of 9',
            'Sprucing up...\\nUnpacking Themes\\nx': 'Preparo...\\nEstrazione: Temi\\nx',
            'Path C:\\temp 12 (a+b)*[c]?': 'Percorso C:\\temp 12 (a+b)*[c]?',
            'Sync stalled\nNo progress for 30s': 'Bloccata\nFermo da 30 s',
            'Themes installed!': 'Temi installato!',
            'Price $5 ^ok': 'Prezzo $5 ^ok',
            'Unknown': 'Unknown',
        }
        with tempfile.TemporaryDirectory() as d:
            f = os.path.join(d, 'Prova.json')
            with open(f, 'w', encoding='utf-8') as fp:
                json.dump(tabella, fp, ensure_ascii=False)
            self.assertEqual(py_traduci(f, list(casi)), list(casi.values()))
            self.assertEqual(jq_traduci(f, list(casi)), list(casi.values()))
            for t, atteso in casi.items():
                self.assertEqual(jq_singolo(f, t), atteso)
            vuoto = os.path.join(d, 'Vuoto.json')
            with open(vuoto, 'w', encoding='utf-8') as fp:
                fp.write('{}')
            self.assertEqual(jq_singolo(vuoto, 'Themes'), 'Themes')

    def test_english_json_allineato_agli_script(self):
        with open(os.path.join(LANG, 'English.json'), encoding='utf-8') as fp:
            en = json.load(fp)['scriptMessages']
        mancanti = sorted(set(estrai_chiavi.stringhe_script()) - set(en))
        self.assertEqual(mancanti, [], 'esegui .github/i18n/sync_english.py')


if __name__ == '__main__':
    unittest.main()
