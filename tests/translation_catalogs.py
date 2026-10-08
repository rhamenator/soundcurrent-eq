# SPDX-License-Identifier: GPL-3.0-only
"""Regression tests for catalog preservation and invalid translation rejection."""
import importlib.util
import json
import re
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location('catalog', Path(__file__).resolve().parents[1] / 'scripts/localization.py')
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)


class CatalogTests(unittest.TestCase):
    def test_studio_owned_diagnostic_gaps_match_current_source(self):
        root = Path(__file__).resolve().parents[1]
        model = root / 'src/studio_model.cpp'
        if not model.exists():
            self.skipTest('Studio session model is not part of EQ')
        reasons = {catalog.literal(args[1]) for args in catalog.calls(model.read_text(), 'require')
                   if len(args) == 2 and catalog.literal(args[1]) is not None}
        boundary = (root / 'src/audio_error_text.h').read_text()
        mappings = dict(re.findall(r'if \(diagnostic == QStringLiteral\("([^"]+)"\)\)\s*return SC_TR\("([^"]+)"\);', boundary))
        gaps = json.loads((root / 'tests/results/localization/studio-model-diagnostic-gaps.json').read_text())
        self.assertEqual(reasons - set(mappings), set(gaps['diagnosticsNeedingMappingAndTranslations']))
        self.assertEqual(gaps['unmappedCount'], len(gaps['diagnosticsNeedingMappingAndTranslations']))
        self.assertTrue({mappings[key] for key in reasons & set(mappings)}.issubset(catalog.marked_sources(root / 'src')))

    def test_bundled_speaker_taxonomy_has_marked_display_labels(self):
        root = Path(__file__).resolve().parents[1]
        profiles = json.loads((root / 'data/equipment/spinorama.json').read_text())
        keys = {profile['equipmentType'] for profile in profiles}
        header = (root / 'src/equipment_display_text.h').read_text()
        mapping = dict(re.findall(r'if\(key=="([^"]+)"\)return SC_TR\("([^"]+)"\);', header))
        self.assertEqual(keys, set(mapping), 'Published taxonomy additions need explicit localized display labels')
        self.assertTrue(set(mapping.values()).issubset(catalog.marked_sources(root / 'src')))

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

    def test_exact_qt_wrapped_captions_cannot_escape_guard(self):
        for wrapper in ['QStringLiteral', 'QString', 'QLatin1String', 'QLatin1StringView']:
            for statement in [f'menu->addAction({wrapper}("Untranslated caption"), callback);',
                              f'combo->setItemText(0, {wrapper}("Untranslated caption"));',
                              f'QMessageBox box(QMessageBox::Warning, {wrapper}("Untranslated caption"), message);']:
                with self.subTest(statement=statement), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    (root / 'src').mkdir()
                    (root / 'src/main.cpp').write_text(statement, encoding='utf-8')
                    with patch.object(catalog, 'ROOT', root), self.assertRaisesRegex(ValueError, 'Unmarked UI literal'):
                        catalog.sources()

    def test_display_literal_preserves_extraction_boundary(self):
        self.assertEqual(catalog.display_literal('QStringLiteral("Saved " "setup")'), 'Saved setup')
        self.assertEqual(catalog.display_literal('QLatin1String("Hz")'), 'Hz')
        for expression in ['SC_TR("Saved setup")', 'QString(SC_TR("Saved setup"))',
                           'QStringLiteral("Device: ") + userName',
                           'QStringLiteral("Channel %1").arg(channel)', 'storedName']:
            self.assertIsNone(catalog.display_literal(expression))
        self.assertIsNone(catalog.literal('QStringLiteral("Backend invariant")'))

    def test_display_inventory_includes_wrappers_and_ignores_comments(self):
        audit_spec = importlib.util.spec_from_file_location('ui_audit', Path(__file__).resolve().parents[1] / 'scripts/ui_string_audit.py')
        audit = importlib.util.module_from_spec(audit_spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            audit_spec.loader.exec_module(audit)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            (root / 'src/new_controls.hpp').write_text(
                'label->setText(QStringLiteral("Unmarked label")); '
                'label->setText(SC_TR("Marked label")); '
                'label->setText(userName); '
                '// label->setText(QStringLiteral("Comment"));', encoding='utf-8')
            result = audit.inventory(root)
            self.assertEqual([row['literal'] for row in result['candidates']], ['Unmarked label'])
            self.assertFalse(result['wholeInterfaceCoverageProven'])

    def test_external_installer_label_is_protected_in_reviewed_instruction(self):
        source = 'The incomplete VB-CABLE installation was removed. Restart Windows, open %1 again, click Install Driver, then restart once more.'
        catalog.validate_text(source, 'Installation retirée. Ouvrez %1 puis cliquez sur Install Driver.')
        for altered in ('Installer le pilote', 'install driver', 'Install Driver Install Driver', ''):
            with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                catalog.validate_text(source, 'Installation retirée. Ouvrez %1 puis cliquez sur ' + altered + '.')
        # This rule does not freeze translatable SoundCurrent-owned action labels.
        catalog.validate_text('Audio driver setup', 'Configuration du pilote audio')

    def test_setup_lookup_keys_have_required_catalog_entries(self):
        root = Path(__file__).resolve().parents[1]
        required = json.loads((root / 'data/localization/setup-sources.json').read_text())
        self.assertEqual(len(required), len(set(required)))
        calls = set()
        for file in ('cable-setup.ps1', 'native-audio-setup.ps1'):
            code = (root / 'packaging/windows' / file).read_text()
            calls.update(re.findall(r"(?:Get|Format)-SCSetupText\s+'([^']+)'", code))
        self.assertEqual(calls, set(required), 'New literal helper lookups must be declared and translated')
        self.assertTrue(set(required).issubset(catalog.sources()))

    def test_setup_export_rejects_unfinished_required_text_before_writing(self):
        root = Path(__file__).resolve().parents[1]
        export_spec = importlib.util.spec_from_file_location('setup_export', root / 'scripts/windows_setup_catalogs.py')
        exporter = importlib.util.module_from_spec(export_spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            export_spec.loader.exec_module(exporter)
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory)
            self.fixture(data)
            (data / 'catalogs.json').write_text(json.dumps([{'tag':'fr'}]))
            (data / 'setup-sources.json').write_text(json.dumps(['First']))
            destination = data / 'export.json'
            with patch.object(catalog, 'DATA', data), patch.object(catalog, 'check'), self.assertRaisesRegex(ValueError, 'Missing required setup translations'):
                exporter.export(destination)
            self.assertFalse(destination.exists())

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
