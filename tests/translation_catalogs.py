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
    def test_native_shared_driver_notice_uses_running_app_wording(self):
        root = Path(__file__).resolve().parents[1]
        source = 'Quit any running SoundCurrent app before changing the shared driver. Removing one app keeps the driver if the other app still uses it.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text()
            if 'native' not in installer.stem:
                self.assertNotIn('SCSharedDriverNotice', code)
            else:
                self.assertRegex(code, r'\$\{NSD_CreateLabel\}[^\n]+"\$\(SCSharedDriverNotice\)"')
                self.assertEqual(re.findall(r'^LangString SCSharedDriverNotice \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text()):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                catalog.validate_text(source, translated.replace('SoundCurrent', 'Other app'))

    def test_cable_restart_notice_preserves_driver_name(self):
        root = Path(__file__).resolve().parents[1]
        source = 'VB-CABLE setup requires a Windows restart. Restart before using the equalizer or opening VB-CABLE settings.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text()
            if 'native' in installer.stem:
                self.assertNotIn('SCCableRestart', code)
            else:
                self.assertRegex(code, r'\$\{NSD_CreateLabel\}[^\n]+"\$\(SCCableRestart\)"')
                self.assertEqual(re.findall(r'^LangString SCCableRestart \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text()):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            for changed in ('Cable', 'vb-cable', 'VB-CABLE VB-CABLE'):
                with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                    catalog.validate_text(source, translated.replace('VB-CABLE', changed))

    def test_installer_driver_check_guidance_uses_translated_action(self):
        root = Path(__file__).resolve().parents[1]
        source = 'Setup could not check the driver. You can retry with %1 in the app or Start menu.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            self.assertRegex(code, r'\$\{NSD_CreateLabel\}[^\n]+"\$\(SCDriverCheckFailed\)"')
            self.assertEqual(re.findall(r'^LangString SCDriverCheckFailed \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source.replace('%1', 'Audio driver setup')])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text()):
            messages = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))
            message = messages[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            self.assertTrue(catalog.finished(messages['Audio driver setup']), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            with self.assertRaisesRegex(ValueError, 'Placeholder mismatch'):
                catalog.validate_text(source, translated.replace('%1', '%2'))

    def test_installer_build_copy_generation_and_failures(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location('installer_build_copy', root / 'scripts/windows_installer_catalogs.py')
        exporter = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(exporter)
        product = 'SoundCurrent Studio' if (root / 'src/studio_model.cpp').exists() else 'SoundCurrent EQ'
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            captions = exporter.export(folder / 'captions.json', product)
            for source in sorted((root / 'packaging/windows').glob('*.nsi')):
                original = source.read_bytes()
                with self.assertRaisesRegex(ValueError, 'product/schema'):
                    exporter.generate_installer(source, folder / 'mismatch.nsi', product, {**captions, 'product': 'Wrong app'})
                generated = folder / ('generated-' + source.name)
                exporter.generate_installer(source, generated, product, captions)
                self.assertEqual(generated.read_bytes(), original, 'Only catalog captions may change; current English is identical')
                self.assertEqual(source.read_bytes(), original)
                with self.assertRaisesRegex(ValueError, 'overwrite'):
                    exporter.generate_installer(source, source, product, captions)
                fixture = folder / source.name
                for replacement in ('', 'LangString SCConnectAudio ${LANG_ENGLISH} "One"\nLangString SCConnectAudio ${LANG_ENGLISH} "Two"'):
                    fixture.write_text(re.sub(r'^LangString SCConnectAudio.*$', replacement, source.read_text(), flags=re.M))
                    failed = folder / 'failed.nsi'
                    with self.assertRaisesRegex(ValueError, 'Missing or duplicate'):
                        exporter.generate_installer(fixture, failed, product, captions)
                    self.assertFalse(failed.exists())
                fixture.write_text(source.read_text() + '\n!insertmacro MUI_LANGUAGE "French"\n')
                with self.assertRaisesRegex(ValueError, 'activation'):
                    exporter.generate_installer(fixture, folder / 'failed.nsi', product, captions)

    def test_installer_export_preserves_literal_names_and_nsis_escaping(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location('installer_export', root / 'scripts/windows_installer_catalogs.py')
        exporter = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(exporter)
        self.assertEqual(exporter.format_names('%2 / %1', 'Driver %2 $1', 'Product %1'), 'Product %1 / Driver %2 $1')
        for template in ('%1', '%1 %1 %2', '%1 %2 %10', '%1 %2 %L1', '%1 %2 %n'):
            with self.assertRaises(ValueError):
                exporter.format_names(template, 'Driver', 'Product')
        self.assertEqual(exporter.nsis_escape('Cost $1 "quoted"\n路径'), 'Cost $$1 $\\"quoted$\\"$\\n路径')
        with self.assertRaises(ValueError):
            exporter.nsis_escape('Bad\x00caption')
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'captions.json'
            exporter.export(output, 'SoundCurrent Studio' if (root / 'src/studio_model.cpp').exists() else 'SoundCurrent EQ')
            result = json.loads(output.read_text())
            self.assertEqual(set(result['languages']), {row['tag'] for row in json.loads((catalog.DATA / 'catalogs.json').read_text())})
            self.assertFalse(result['installerLocaleActivationComplete'])
            for variants in result['languages'].values():
                self.assertEqual(set(variants), {'cable', 'native'})
                for routeName, route in variants.items():
                    self.assertEqual(set(route['captions']), {'SCConnectAudio', 'SCSetupAudio', 'SCInstallDriver', 'SCDriverCheckFailed'} | ({'SCCableRestart'} if routeName == 'cable' else {'SCSharedDriverNotice'}))
                    self.assertEqual(set(route['nsisEscaped']), set(route['captions']))

    def test_installer_checkbox_catalog_sources_and_names(self):
        root = Path(__file__).resolve().parents[1]
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            native = 'native' in installer.stem
            source = ('Install or update the shared SoundCurrent Audio driver' if native
                      else 'Install VB-CABLE if missing (administrator approval)')
            name = 'SoundCurrent Audio' if native else 'VB-CABLE'
            code = installer.read_text(encoding='utf-8')
            self.assertRegex(code, r'\$\{NSD_CreateCheckbox\}[^\n]+"\$\(SCInstallDriver\)"')
            self.assertEqual(re.findall(r'^LangString SCInstallDriver \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
            for row in json.loads((catalog.DATA / 'catalogs.json').read_text()):
                message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
                self.assertTrue(catalog.finished(message), row['tag'])
                translated = message.findtext('translation')
                catalog.validate_text(source, translated)
                for replacement in ('Wrong driver', name.lower(), name + ' ' + name):
                    with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                        catalog.validate_text(source, translated.replace(name, replacement))

    def test_installer_setup_subtitle_preserves_product_names(self):
        root = Path(__file__).resolve().parents[1]
        template = 'Set up %1 for %2.'
        product = 'SoundCurrent Studio' if (root / 'src/studio_model.cpp').exists() else 'SoundCurrent EQ'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            driver = 'SoundCurrent Audio' if 'native' in installer.stem else 'VB-CABLE'
            code = installer.read_text(encoding='utf-8')
            self.assertIn('MUI_HEADER_TEXT "$(SCConnectAudio)" "$(SCSetupAudio)"', code)
            values = re.findall(r'^LangString SCSetupAudio \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M)
            self.assertEqual(values, [template.replace('%1', driver).replace('%2', product)])
            for row in json.loads((catalog.DATA / 'catalogs.json').read_text()):
                message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[template]
                self.assertTrue(catalog.finished(message), row['tag'])
                translated = message.findtext('translation')
                catalog.validate_text(template, translated)
                rendered = translated.replace('%1', driver).replace('%2', product)
                self.assertEqual(rendered.count(driver), 1)
                self.assertEqual(rendered.count(product), 1)

    def test_installer_audio_heading_matches_catalog_source(self):
        root = Path(__file__).resolve().parents[1]
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            self.assertIn('MUI_HEADER_TEXT "$(SCConnectAudio)"', code)
            matches = re.findall(r'^LangString SCConnectAudio \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M)
            self.assertEqual(matches, ['Connect your audio'])
            self.assertIn(matches[0], catalog.sources())
            for row in json.loads((catalog.DATA / 'catalogs.json').read_text()):
                entry = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[matches[0]]
                self.assertTrue(catalog.finished(entry), row['tag'])

    def test_declared_helper_source_is_extracted_without_seed(self):
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory)
            (data / 'seed-translations.json').write_bytes((catalog.DATA / 'seed-translations.json').read_bytes())
            fixture = 'New owned helper diagnostic %1'
            self.assertNotIn(fixture, json.loads((data / 'seed-translations.json').read_text())['sources'])
            (data / 'setup-sources.json').write_text(json.dumps([fixture]))
            with patch.object(catalog, 'DATA', data):
                self.assertIn(fixture, catalog.sources())
        # Use the actual existing catalogs: a newly declared source must invalidate
        # them before helper export, even if no seed translation was added.
        declared = catalog.setup_sources() | {fixture}
        with patch.object(catalog, 'setup_sources', return_value=declared):
            with self.assertRaisesRegex(ValueError, 'Run --update after UI changes'):
                catalog.check(require_complete=True)

    def test_setup_source_inventory_rejects_damaged_declarations(self):
        for values in ({'source': 'Text'}, ['Text', 'Text'], [''], ['  '], [None], [42], [['Text']]):
            with self.subTest(values=values), tempfile.TemporaryDirectory() as directory:
                data = Path(directory)
                (data / 'setup-sources.json').write_text(json.dumps(values))
                with patch.object(catalog, 'DATA', data), self.assertRaisesRegex(ValueError, 'Invalid setup source inventory'):
                    catalog.setup_sources()

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

    def test_repair_failure_external_labels(self):
        source = 'VB-CABLE still has no usable playback/recording endpoints. Complete Remove Driver in the official setup, restart Windows, then open %1 again to reinstall. Windows Sound settings must have CABLE Input and CABLE Output enabled.'
        valid = 'Terminez Remove Driver, redémarrez et ouvrez %1. Activez CABLE Input et CABLE Output.'
        catalog.validate_text(source, valid)
        for label in ('Remove Driver', 'CABLE Input', 'CABLE Output'):
            for replacement in ('Traduction', label.lower(), label + ' ' + label):
                with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                    catalog.validate_text(source, valid.replace(label, replacement))
        with self.assertRaisesRegex(ValueError, 'Placeholder mismatch'):
            catalog.validate_text(source, valid.replace('%1', '%2'))

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

    def test_reviewed_installer_reference_requires_language_definitions(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location('nsis_definitions', root / 'scripts/nsis_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(audit)
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            folder = fixture / 'packaging/windows'
            folder.mkdir(parents=True)
            source = folder / 'fixture.nsi'
            base = '!insertmacro MUI_LANGUAGE "English"\n!insertmacro MUI_LANGUAGE "French"\nMessageBox MB_OK "$(Caption)"\n'
            valid = 'LangString Caption ${LANG_ENGLISH} "Caption"\nLangString Caption ${LANG_FRENCH} "Légende"\n'
            source.write_text(base + valid, encoding='utf-8')
            reviewed = audit.inventory(fixture)
            audit.check_backlog(reviewed, reviewed)
            for definitions in ('', valid.splitlines()[0] + '\n', valid.replace('"Légende"', '" "'), valid + valid):
                source.write_text(base + definitions, encoding='utf-8')
                with self.subTest(definitions=definitions), self.assertRaisesRegex(ValueError, 'installer language definition'):
                    audit.check_backlog(audit.inventory(fixture), reviewed)

    def test_installer_source_backlog_and_regression_gate(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location('nsis_audit', root / 'scripts/nsis_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(audit)
        audit.check_backlog(audit.inventory(root), json.loads((root / 'data/localization/nsis-text-backlog.json').read_text()))
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            folder = fixture / 'packaging/windows'
            folder.mkdir(parents=True)
            source = folder / 'fixture.nsi'
            source.write_text('; MessageBox MB_OK "Ignored comment"\n'
                              '/* DetailPrint "Ignored block" */\n'
                              '${NSD_CreateLabel} 0 0 100% 10u "Caption; # literal"\n'
                              'MessageBox MB_OK "Quoted $\\"word$\\""\n'
                              'DetailPrint $1\n'
                              '!insertmacro MUI_LANGUAGE "English"\n', encoding='utf-8')
            baseline = audit.inventory(fixture)
            self.assertEqual([row['literal'] for row in baseline['candidates']], ['Caption; # literal', 'Quoted $\\"word$\\"'])
            self.assertEqual(len(baseline['dynamicSites']), 1)
            self.assertFalse(baseline['wholeInterfaceCoverageProven'])
            with source.open('a') as file:
                file.write('MessageBox MB_OK "New untranslated installer message"\n')
            with self.assertRaisesRegex(ValueError, 'New untranslated installer text'):
                audit.check_backlog(audit.inventory(fixture), baseline)
            source.write_text('MessageBox MB_OK "$(UnreviewedCaption)"\n', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Unaudited installer language reference'):
                audit.check_backlog(audit.inventory(fixture), baseline)

    def test_remove_driver_label_survives_instruction_translation(self):
        sources = ['Remove the shared VB-CABLE driver too? Other users, recording apps, or voice tools may need it. Confirm to open the official remover, then click Remove Driver. Decline to keep the cable and uninstall only SoundCurrent.', 'VB-CABLE is still present. If removal requested a restart, restart Windows and retry SoundCurrent uninstall; otherwise finish Remove Driver in the official setup.']
        for source in sources:
            catalog.validate_text(source, 'Cliquez sur Remove Driver.')
            for target in ('Supprimer le pilote', 'remove driver', 'Remove Driver Remove Driver'):
                with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                    catalog.validate_text(source, target)

    def test_endpoint_device_labels_remain_exact(self):
        source = 'Windows has a VB-CABLE driver record, but its playback or recording endpoint is unavailable. If you have already restarted, open %1 to repair it. Enable CABLE Input and CABLE Output in Windows Sound settings if they are disabled.'
        catalog.validate_text(source, 'Ouvrez %1. Activez CABLE Input et CABLE Output.')
        for text in ('CABLE input et CABLE Output', 'Entrée câble et CABLE Output', 'CABLE Input et Sortie câble', 'CABLE Input et CABLE Output CABLE Output'):
            with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                catalog.validate_text(source, 'Ouvrez %1. ' + text)

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
