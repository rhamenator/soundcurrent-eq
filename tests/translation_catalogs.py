# SPDX-License-Identifier: GPL-3.0-only
"""Regression tests for catalog preservation and invalid translation rejection."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location('catalog', Path(__file__).resolve().parents[1] / 'scripts/localization.py')
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)


class CatalogTests(unittest.TestCase):
    def test_extraction_respects_comments_and_nested_arguments(self):
        code = '// SC_TR("Not UI")\n/* SC_TR("Not UI either") */\nSC_TR("A " "label"); require(call(a, b), "A reason"); text("Value %1");'
        self.assertEqual([catalog.literal(args[0]) for args in catalog.calls(code, 'SC_TR')], ['A label'])
        self.assertEqual([catalog.literal(args[1]) for args in catalog.calls(code, 'require')], ['A reason'])

    def test_marked_sources_include_unlisted_platform_adapters(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'platform').mkdir()
            (root / 'platform/audio.inc').write_text('SC_TR("Route unavailable"); // SC_TR("Comment")', encoding='utf-8')
            (root / 'new_controls.hpp').write_text('SC_TR("New control");', encoding='utf-8')
            (root / 'notes.txt').write_text('SC_TR("Not compiled source");', encoding='utf-8')
            self.assertEqual(catalog.marked_sources(root), {'Route unavailable', 'New control'})

    def test_source_scan_rejects_unmarked_caption(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            (root / 'src/main.cpp').write_text('finish("English only", true);', encoding='utf-8')
            with patch.object(catalog, 'ROOT', root), self.assertRaisesRegex(ValueError, 'Unmarked UI literal'):
                catalog.sources()

    def test_tray_and_combo_captions_require_markers(self):
        for code in ['menu->addAction("English only", callback);',
                     'combo->setItemText(0, "English only");']:
            with self.subTest(code=code), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'src').mkdir()
                (root / 'src/main.cpp').write_text(code, encoding='utf-8')
                with patch.object(catalog, 'ROOT', root), self.assertRaisesRegex(ValueError, 'Unmarked UI literal'):
                    catalog.sources()

    def test_new_control_header_cannot_escape_caption_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            (root / 'src/new_controls.hpp').write_text('menu->addAction("Untranslated new control", callback);', encoding='utf-8')
            with patch.object(catalog, 'ROOT', root), self.assertRaisesRegex(ValueError, 'Unmarked UI literal'):
                catalog.sources()

    def test_named_dialog_title_requires_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            (root / 'src/main.cpp').write_text('QMessageBox preview(QMessageBox::Warning, "Untranslated title", message);', encoding='utf-8')
            with patch.object(catalog, 'ROOT', root), self.assertRaisesRegex(ValueError, 'dialog title'):
                catalog.sources()

    def test_windows_adapter_errors_require_translation_markers(self):
        with self.assertRaisesRegex(ValueError, 'Unmarked Windows adapter error'):
            catalog.check_platform_errors('throw std::runtime_error("Route unavailable");')
        catalog.check_platform_errors('throw std::runtime_error(SC_TR("Route unavailable").toStdString());')
        catalog.check_platform_errors('throw std::runtime_error(backend.error());')

    def test_structural_translation_checks(self):
        valid = [('%1 / %2', '%2 / %1'), ('%L1 / %n', '%n / %L1'),
                 ('Settings && calibration', 'Réglages && étalonnage'),
                 ('WAVE (*.wav)', 'Audio WAVE (*.wav)'),
                 ('<a href="https://example.invalid/">Source</a>', '<a href="https://example.invalid/">Référence</a>')]
        for source, target in valid:
            catalog.validate_text(source, target)
        invalid = [('%1 / %1', '%1'), ('%L1', '%1'), ('%Ln', '%n'),
                   ('Settings && calibration', 'Réglages & étalonnage'),
                   ('WAVE (*.wav)', 'Audio (*.mp3)'),
                   ('Files (*.wav);;All (*)', 'Fichiers (*.wav)'),
                   ('<a href="https://example.invalid/">Source</a>', '<a href="https://other.invalid/">Source</a>'),
                   ('Value', '\u202eValeur'), ('Value', ' ')]
        for source, target in invalid:
            with self.subTest(source=source, target=target), self.assertRaises(ValueError):
                catalog.validate_text(source, target)

    def fixture(self, data):
        (data / 'seed-translations.json').write_text(json.dumps({'sources': ['First'], 'languages': {'fr': ['Français', 'Seed must not overwrite work']}}), encoding='utf-8')
        (data / 'translation-context.json').write_text(json.dumps({'First': 'Audio meaning'}), encoding='utf-8')
        (data / 'soundcurrent_fr.ts').write_text('''<TS language="fr"><context><name>SoundCurrent</name><message><source>First</source><translation type="unfinished">Travail en cours</translation><translatorcomment>Ask native reviewer</translatorcomment></message><message><source>Second</source><translation>Deuxième</translation></message></context></TS>''', encoding='utf-8')

    def test_update_preserves_unfinished_text_and_translator_notes(self):
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory)
            self.fixture(data)
            def fake_compile(command, **kwargs):
                Path(command[command.index('-qm') + 1]).write_bytes(b'test-compiler-output')
            with patch.object(catalog, 'DATA', data), patch.object(catalog, 'sources', return_value=['First', 'Second']), patch.object(catalog.shutil, 'which', return_value='test-compiler'), patch.object(catalog.subprocess, 'run', side_effect=fake_compile):
                catalog.update()
                result = catalog.entries(data / 'soundcurrent_fr.ts')
                self.assertEqual(result['First'].findtext('translation'), 'Travail en cours')
                self.assertEqual(result['First'].find('translation').get('type'), 'unfinished')
                self.assertEqual(result['First'].findtext('translatorcomment'), 'Ask native reviewer')
                self.assertEqual(result['First'].findtext('extracomment'), 'Audio meaning')
                catalog.check()
                with self.assertRaisesRegex(ValueError, 'unfinished'):
                    catalog.check(require_complete=True)
                (data / 'soundcurrent_fr.qm').write_bytes(b'tampered')
                with self.assertRaisesRegex(ValueError, 'Stale QM'):
                    catalog.check()

    def test_numerus_rejected_before_any_catalog_is_rewritten(self):
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory)
            self.fixture(data)
            path = data / 'soundcurrent_fr.ts'
            path.write_text('<TS><context><name>SoundCurrent</name><message numerus="yes"><source>%n tracks</source><translation><numerusform>%n piste</numerusform><numerusform>%n pistes</numerusform></translation></message></context></TS>', encoding='utf-8')
            before = {p.name: p.read_bytes() for p in data.iterdir()}
            with patch.object(catalog, 'DATA', data), patch.object(catalog, 'sources', return_value=['First', 'Second']), patch.object(catalog.shutil, 'which', return_value='test-compiler'), self.assertRaisesRegex(ValueError, 'Numerus'):
                catalog.update()
            self.assertEqual(before, {p.name: p.read_bytes() for p in data.iterdir()})


if __name__ == '__main__':
    unittest.main()
