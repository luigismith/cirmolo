"""Test delle traduzioni di PyUI eseguibili sul PC (senza la console).

Uso (dalla radice del repo):
    python -I -X utf8 -m unittest discover -s .github/i18n/tests -v
"""
import json
import logging
import os
import sys
import unittest

RADICE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
MAIN_UI = os.path.join(RADICE, 'App', 'PyUI', 'main-ui')
LANG = os.path.join(RADICE, 'App', 'PyUI', 'lang')
sys.path.insert(0, MAIN_UI)

from utils.logger import PyUiLogger  # noqa: E402

PyUiLogger._logger = logging.getLogger('i18n-test')
from menus.language.language import Language  # noqa: E402

sys.path.insert(0, os.path.join(RADICE, '.github', 'i18n'))
import estrai_chiavi  # noqa: E402


def carica(lingua):
    Language._data = {}
    Language._read_from_file(os.path.join(LANG, lingua + '.json'))


class TestEnglishInvariato(unittest.TestCase):
    """L'inglese deve restare identico ai default scritti nel codice."""

    def test_default_del_codice_uguali_a_english_json(self):
        """Ogni chiave usata dal codice esiste in English.json; quelle aggiunte da noi (assenti
        nell'English.json originale di spruceOS) hanno esattamente il testo del default del codice."""
        import subprocess
        with open(os.path.join(LANG, 'English.json'), encoding='utf-8') as f:
            en = json.load(f)
        try:
            originale = json.loads(subprocess.run(
                ['git', '-C', RADICE, 'show', 'upstream/Development:App/PyUI/lang/English.json'],
                capture_output=True, check=True).stdout.decode('utf-8'))
        except (OSError, subprocess.CalledProcessError, ValueError):
            originale = None
        for chiave, info in estrai_chiavi.chiavi_dal_codice().items():
            with self.subTest(chiave=chiave):
                self.assertIn(chiave, en)
                if originale is not None and chiave not in originale:
                    self.assertEqual(en[chiave], info['default'])

    def test_translate_inglese_restituisce_lo_stesso_testo(self):
        carica('English')
        for sezione, voci in estrai_chiavi.stringhe_display().items():
            for testo in voci:
                with self.subTest(sezione=sezione, testo=testo):
                    self.assertEqual(Language.translate(sezione, testo), testo)


class TestItaliano(unittest.TestCase):
    def setUp(self):
        carica('Italian')

    def test_nessuna_chiave_del_codice_ricade_in_inglese(self):
        for chiave in estrai_chiavi.chiavi_dal_codice():
            with self.subTest(chiave=chiave):
                self.assertIn(chiave, Language._data)

    def test_booleani_e_attivo(self):
        self.assertEqual(Language.boolean_label(True), 'Sì')
        self.assertEqual(Language.boolean_label(False), 'No')
        self.assertEqual(Language.on_off_label(True), 'Attivo')
        self.assertEqual(Language.menu_option_value('True'), 'Sì')

    def test_app_etichette_e_descrizioni(self):
        self.assertEqual(Language.app_label('File Management'), 'Gestione file')
        self.assertEqual(Language.app_description('Transfer files over USB'), 'Trasferisci file via USB')
        self.assertEqual(Language.app_label('App sconosciuta'), 'App sconosciuta')
        self.assertEqual(Language.app_label(None), None)

    def test_modelli_con_segnaposto(self):
        self.assertEqual(Language.app_description('Version 4.5.4 is available'), 'È disponibile la versione 4.5.4')
        self.assertEqual(Language.app_description('A new build of version 4.5.4 (abc1234) is available'),
                         'È disponibile una nuova build della versione 4.5.4 (abc1234)')
        self.assertEqual(Language.page_title('SNES Search'), 'Cerca in SNES')
        self.assertEqual(Language.page_title('Favorites'), 'Preferiti')
        self.assertEqual(Language.page_title('SNES'), 'SNES')

    def test_strumenti(self):
        self.assertEqual(Language.task_label('Repair SD Card'), 'Ripara la scheda SD')
        self.assertEqual(Language.task_description('delete your favorited games list'),
                         "cancella l'elenco dei giochi preferiti")

    def test_descrizioni_impostazioni_reggono_str_format(self):
        for testo in (Language._data.get('menuOptionDescriptions') or {}).values():
            with self.subTest(testo=testo):
                testo.format(ip_addr='192.168.1.2')

    def test_formato_data_salvaschermo(self):
        giorni = Language._data['dateWeekdays']
        mesi = Language._data['dateMonths']
        self.assertEqual(len(giorni), 7)
        self.assertEqual(len(mesi), 12)
        self.assertEqual(Language._data['screensaverDateFormat'].format(weekday=giorni[0], day=5, month=mesi[9]),
                         'Lunedì 5 ottobre')

    def test_selettore_lingua(self):
        self.assertEqual(Language._data.get('displayName'), 'Italiano')


if __name__ == '__main__':
    unittest.main()
