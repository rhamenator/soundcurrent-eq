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
    def test_all_files_filter_preserves_wildcard_and_filter_count(self):
        source='All files (*)'
        catalog.validate_text(source,'Tous les fichiers (*)')
        for translated in ['Tous les fichiers','Tous les fichiers (*.json)','Tous les fichiers (*) ;; JSON (*.json)']:
            with self.subTest(translated=translated):
                with self.assertRaisesRegex(ValueError,'File-dialog filter patterns changed'):
                    catalog.validate_text(source,translated)

    def test_cli_help_preserves_reviewed_limits_and_identifiers(self):
        for source, tokens in catalog.REVIEWED_CLI_TOKENS.items():
            translated = 'Texte ' + ' '.join(tokens) + ' ' + ' '.join(catalog.PLACEHOLDER.findall(source))
            catalog.validate_text(source, translated)
            for token in tokens:
                with self.subTest(source=source, token=token):
                    with self.assertRaisesRegex(ValueError, 'CLI invariant changed'):
                        catalog.validate_text(source, translated.replace(token, '', 1))
                    with self.assertRaisesRegex(ValueError, 'CLI invariant changed'):
                        catalog.validate_text(source, translated + ' ' + token)
                    extended = token + ('0' if any(c.isdigit() for c in token) else 'x')
                    with self.assertRaisesRegex(ValueError, 'CLI invariant changed'):
                        catalog.validate_text(source, translated.replace(token, extended, 1))

    def test_welcome_paragraphs_keep_update_and_background_meaning(self):
        root = Path(__file__).resolve().parents[1]
        product = 'SoundCurrent Studio' if (root / 'src/studio_model.cpp').exists() else 'SoundCurrent EQ'
        first = 'Install or update %1. You do not need to uninstall an older version. Your settings, presets and equipment profiles will be kept.'
        second = 'Save your work and quit the running app before continuing. Closing its window keeps it running in the background.'
        spec = importlib.util.spec_from_file_location('welcome_export', root / 'scripts/windows_installer_catalogs.py')
        exporter = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(exporter)
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            self.assertIn('!define MUI_WELCOMEPAGE_TEXT "$(SCWelcome)"', code)
            expected = first.replace('%1', product) + r'$\r$\n$\r$\n' + second
            self.assertEqual(re.findall(r'^LangString SCWelcome \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [expected])
            self.assertNotIn('use Quit to exit', code)
        with tempfile.TemporaryDirectory() as folder:
            result = exporter.export(Path(folder) / 'welcome.json', product)
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            messages = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))
            for source in (first,second):
                self.assertTrue(catalog.finished(messages[source]), row['tag'])
                translated = messages[source].findtext('translation')
                catalog.validate_text(source, translated)
                self.assertNotIn('\n', translated)
                self.assertNotIn('\r', translated)
            for data in result['languages'][row['tag']].values():
                raw = data['captions']['SCWelcome']
                paragraphs = raw.split('\r\n\r\n')
                self.assertEqual(len(paragraphs),2)
                self.assertEqual(paragraphs[0], exporter.format_value(messages[first].findtext('translation'), product))
                self.assertEqual(paragraphs[1], messages[second].findtext('translation'))
                self.assertEqual(raw.count(product),1)
                escaped = data['nsisEscaped']['SCWelcome']
                self.assertEqual(escaped.count(r'$\r$\n'),2)
        spec = importlib.util.spec_from_file_location('welcome_audit', root / 'scripts/nsis_string_audit.py')
        audit = importlib.util.module_from_spec(spec); spec.loader.exec_module(audit)
        found = audit.inventory(root)
        owned = [row for row in found['candidates'] if row['kind'] != 'Section']
        self.assertTrue(owned)
        self.assertTrue(all(row['marked'] for row in owned), owned)
        audit.check_backlog(found, json.loads((catalog.DATA / 'nsis-text-backlog.json').read_text(encoding='utf-8')))

    def test_setup_failure_keeps_actual_shortcut_and_parameter_roles(self):
        root = Path(__file__).resolve().parents[1]
        product = 'SoundCurrent Studio' if (root / 'src/studio_model.cpp').exists() else 'SoundCurrent EQ'
        source = '%1 setup did not finish. %2 itself is installed. Use %3 in the Start menu to retry; see setup details for the reason.'
        spec = importlib.util.spec_from_file_location('setup_failure_export', root / 'scripts/windows_installer_catalogs.py')
        exporter = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(exporter)
        self.assertEqual(exporter.format_values('%3 / %1 / %2', ('Driver %2', 'App $1', 'Action %1')), 'Action %1 / Driver %2 / App $1')
        for malformed in ('%1 %2', '%1 %2 %2', '%1 %2 %4', '%1 %2 %L3', '%1 %2 %n'):
            with self.assertRaisesRegex(ValueError, 'parameter mismatch'):
                exporter.format_values(malformed, ('Driver',product,'Action'))
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            driver = 'SoundCurrent Audio' if 'native' in installer.stem else 'VB-CABLE'
            self.assertIn('MessageBox MB_OK|MB_ICONINFORMATION "$(SCSetupFailedAppInstalled)"', code)
            self.assertNotIn('SoundCurrent Audio was not installed.', code)
            expected = exporter.format_values(source, (driver, product, 'Audio driver setup'))
            self.assertEqual(re.findall(r'^LangString SCSetupFailedAppInstalled \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [expected])
            self.assertRegex(code, r'CreateShortcut "[^"]+Audio driver setup\.lnk"')
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            for driver in ('VB-CABLE','SoundCurrent Audio'):
                result = exporter.format_values(translated, (driver, product, 'Audio driver setup'))
                for actualName in (driver, product, 'Audio driver setup'):
                    self.assertEqual(result.count(actualName), 1)
            with self.assertRaisesRegex(ValueError, 'Placeholder mismatch'):
                catalog.validate_text(source, translated.replace('%3','%2'))
        spec = importlib.util.spec_from_file_location('all_messagebox_audit', root / 'scripts/nsis_string_audit.py')
        audit = importlib.util.module_from_spec(spec); spec.loader.exec_module(audit)
        found = audit.inventory(root)
        dialogs = [row for row in found['candidates'] if row['kind'] == 'MessageBox']
        self.assertTrue(dialogs)
        self.assertTrue(all(row['marked'] for row in dialogs), dialogs)
        audit.check_backlog(found, json.loads((catalog.DATA / 'nsis-text-backlog.json').read_text(encoding='utf-8')))

    def test_installer_restart_dialog_reuses_reviewed_notice(self):
        root = Path(__file__).resolve().parents[1]
        source = 'VB-CABLE setup requires a Windows restart. Restart before using the equalizer or opening VB-CABLE settings.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' in installer.stem:
                self.assertNotIn('MessageBox MB_OK|MB_ICONINFORMATION "$(SCCableRestart)"', code)
            else:
                self.assertIn('MessageBox MB_OK|MB_ICONINFORMATION "$(SCCableRestart)" /SD IDOK', code)
                self.assertIn('${If} $0 == 3010\n      SetRebootFlag true', code)
                self.assertEqual(re.findall(r'^LangString SCCableRestart \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            catalog.validate_text(source, message.findtext('translation'))

    def test_cable_removal_keeps_reviewed_helper_suffix_and_abort(self):
        root = Path(__file__).resolve().parents[1]
        source = 'VB-CABLE removal did not finish. This app was kept so you can retry.'
        suffix = r'$\r$\n$1'
        reference = '$(SCCableRemovalFailed)' + suffix
        spec = importlib.util.spec_from_file_location('removal_suffix_audit', root / 'scripts/nsis_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(audit)
        self.assertEqual(audit.language_reference(reference), 'SCCableRemovalFailed')
        for unsafe in ('$(SCCableRemovalFailed) English prose', reference+' extra', reference+'$2', '$(SCCableRemovalFailed)$1', '$(SCCableRemovalFailed)'+suffix.replace('$1','$2')):
            self.assertIsNone(audit.language_reference(unsafe))
        found = audit.inventory(root)
        audit.check_backlog(found, json.loads((catalog.DATA / 'nsis-text-backlog.json').read_text(encoding='utf-8')))
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' in installer.stem:
                self.assertNotIn('SCCableRemovalFailed', code)
            else:
                self.assertIn('MessageBox MB_ICONEXCLAMATION "'+reference+'"\n    Abort', code)
                self.assertEqual(re.findall(r'^LangString SCCableRemovalFailed \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            self.assertNotIn('$1', translated)
            with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                catalog.validate_text(source, translated.replace('VB-CABLE','Another cable'))
        # A reviewed composite reference still needs a real declaration.
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory); folder = fixture / 'packaging/windows'; folder.mkdir(parents=True)
            file = folder / 'fixture.nsi'
            file.write_text('!insertmacro MUI_LANGUAGE "English"\nLangString SCCableRemovalFailed ${LANG_ENGLISH} "Failure"\nMessageBox MB_OK "'+reference+'"\n', encoding='utf-8')
            baseline = audit.inventory(fixture); audit.check_backlog(baseline, baseline)
            file.write_text('!insertmacro MUI_LANGUAGE "English"\nMessageBox MB_OK "'+reference+'"\n', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Missing, empty or duplicate'):
                audit.check_backlog(audit.inventory(fixture), baseline)

    def test_native_removal_error_uses_running_apps_and_aborts(self):
        root = Path(__file__).resolve().parents[1]
        source = 'Shared audio driver removal did not finish. This app was kept so you can retry. Quit any running SoundCurrent app, then retry uninstalling.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' not in installer.stem:
                self.assertNotIn('SCNativeRemovalFailed', code)
            else:
                message = 'MessageBox MB_ICONEXCLAMATION "$(SCNativeRemovalFailed)"'
                self.assertIn(message+'\n    Abort', code)
                self.assertEqual(re.findall(r'^LangString SCNativeRemovalFailed \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
                self.assertNotIn('Quit EQ and Studio, then retry uninstalling.', code)
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                catalog.validate_text(source, translated.replace('SoundCurrent','Another app'))

    def test_setup_progress_keeps_route_names_and_no_raw_static_details(self):
        root = Path(__file__).resolve().parents[1]
        sources = {'SCNativeSetupProgress': 'Setting up the shared %1 driver...',
                   'SCCableSetupProgress': 'Opening %1 setup...',
                   'SCSetupRetryProgress': '%1 setup did not finish. Retry using the Start menu shortcut.'}
        spec = importlib.util.spec_from_file_location('setup_progress_audit', root / 'scripts/nsis_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(audit)
        found = audit.inventory(root)
        details = [row for row in found['candidates'] if row['kind'] == 'DetailPrint']
        self.assertTrue(details)
        self.assertTrue(all(row['marked'] for row in details), details)
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            native = 'native' in installer.stem
            driver = 'SoundCurrent Audio' if native else 'VB-CABLE'
            selected = ('SCNativeSetupProgress', 'SCSetupRetryProgress') if native else ('SCCableSetupProgress', 'SCSetupRetryProgress')
            for key in selected:
                self.assertIn('DetailPrint "$('+key+')"', code)
                self.assertEqual(re.findall(r'^LangString '+key+r' \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [sources[key].replace('%1',driver)])
            self.assertNotIn('SCCableSetupProgress' if native else 'SCNativeSetupProgress', code)
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            messages = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))
            for source in sources.values():
                self.assertTrue(catalog.finished(messages[source]), row['tag'])
                translated = messages[source].findtext('translation')
                catalog.validate_text(source, translated)
                with self.assertRaisesRegex(ValueError, 'Placeholder mismatch'):
                    catalog.validate_text(source, translated.replace('%1','%2'))

    def test_quit_installer_messages_keep_product_placeholder(self):
        root = Path(__file__).resolve().parents[1]
        product = 'SoundCurrent Studio' if (root / 'src/studio_model.cpp').exists() else 'SoundCurrent EQ'
        sources = {'SCQuitBeforeUpdate': 'Quit %1 before updating. Closing the window keeps it running. No uninstall is needed.',
                   'SCQuitBeforeUninstall': 'Quit %1 before uninstalling it.'}
        spec = importlib.util.spec_from_file_location('quit_installer_export', root / 'scripts/windows_installer_catalogs.py')
        exporter = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(exporter)
        self.assertEqual(exporter.format_value('Quit %1.', 'Name %1 / $1'), 'Quit Name %1 / $1.')
        for malformed in ('Quit.', '%1 %1', '%2', '%10', '%L1'):
            with self.assertRaisesRegex(ValueError, 'exactly one %1'):
                exporter.format_value(malformed, product)
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            for key, source in sources.items():
                self.assertIn('MessageBox MB_ICONEXCLAMATION "$('+key+')"', code)
                self.assertEqual(re.findall(r'^LangString '+key+r' \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source.replace('%1', product)])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            messages = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))
            for source in sources.values():
                self.assertTrue(catalog.finished(messages[source]), row['tag'])
                translated = messages[source].findtext('translation')
                catalog.validate_text(source, translated)
                self.assertIn(product, exporter.format_value(translated, product))
                with self.assertRaisesRegex(ValueError, 'Placeholder mismatch'):
                    catalog.validate_text(source, translated.replace('%1','%2'))

    def test_cable_routing_notice_and_custom_page_coverage(self):
        root = Path(__file__).resolve().parents[1]
        source = 'VB-CABLE routes playback through the app. Choose speakers inside SoundCurrent. VB-CABLE is VB-Audio donationware: https://vb-cable.com — donations are welcome.'
        spec = importlib.util.spec_from_file_location('audio_page_audit', root / 'scripts/nsis_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(audit)
        found = audit.inventory(root)
        controls = [row for row in found['candidates'] if row['kind'] == 'control-caption']
        self.assertTrue(controls)
        self.assertTrue(all(row['marked'] for row in controls), controls)
        audit.check_backlog(found, json.loads((catalog.DATA / 'nsis-text-backlog.json').read_text(encoding='utf-8')))
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' in installer.stem:
                self.assertNotIn('SCCableRouting', code)
            else:
                self.assertIn('${NSD_CreateLabel} 0 0 100% 32u "$(SCCableRouting)"', code)
                self.assertEqual(re.findall(r'^LangString SCCableRouting \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            for external in ('VB-CABLE', 'SoundCurrent', 'VB-Audio', 'https://vb-cable.com'):
                with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                    catalog.validate_text(source, translated.replace(external, 'Other label'))

    def test_shared_cable_notice_keeps_family_and_optional_cables(self):
        root = Path(__file__).resolve().parents[1]
        source = 'Quit any running equalizer before driver setup. When removing the last SoundCurrent app, its uninstaller offers VB-CABLE removal. Other software may also need the cable. Extra A/B cables are not bundled.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' in installer.stem:
                self.assertNotIn('SCCableSharedNotice', code)
            else:
                self.assertIn('${NSD_CreateLabel} 0 108u 100% 40u "$(SCCableSharedNotice)"', code)
                self.assertEqual(re.findall(r'^LangString SCCableSharedNotice \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            for external in ('SoundCurrent', 'VB-CABLE', 'A/B'):
                with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                    catalog.validate_text(source, translated.replace(external, 'Other label'))

    def test_setup_dialog_test_logs_exact_utf8(self):
        root = Path(__file__).resolve().parents[1]
        code = (root / 'src/windows_audio_setup_test.inc').read_text(encoding='utf-8')
        self.assertIn('text.contains(expected, Qt::CaseInsensitive)', code)
        self.assertNotIn('qPrintable', code)
        for value in ('action', 'expected', 'text'):
            self.assertIn(value + '.toUtf8().toHex().constData()', code)

    def test_signed_installer_notice_preserves_external_button(self):
        root = Path(__file__).resolve().parents[1]
        source = 'Setup opens VB-Audio’s signed installer. Click Install Driver, then restart Windows before using the equalizer or VB-CABLE settings.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' in installer.stem:
                self.assertNotIn('SCCableSignedInstaller', code)
            else:
                self.assertIn('${NSD_CreateLabel} 0 65u 100% 35u "$(SCCableSignedInstaller)"', code)
                self.assertEqual(re.findall(r'^LangString SCCableSignedInstaller \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            for external in ('VB-Audio', 'Install Driver', 'Windows', 'VB-CABLE'):
                with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                    catalog.validate_text(source, translated.replace(external, 'Other label'))

    def test_incomplete_driver_notice_keeps_brand_and_route(self):
        root = Path(__file__).resolve().parents[1]
        source = 'VB-CABLE has a driver record but no usable audio endpoints. Setup offers repair: remove the driver, restart, reinstall, and restart again.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' in installer.stem:
                self.assertNotIn('SCCableRepair', code)
            else:
                self.assertIn('${NSD_CreateLabel} 0 65u 100% 35u "$(SCCableRepair)"', code)
                self.assertEqual(re.findall(r'^LangString SCCableRepair \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                catalog.validate_text(source, translated.replace('VB-CABLE', 'Another cable'))

    def test_existing_cable_notice_keeps_names_and_quit_placeholder(self):
        root = Path(__file__).resolve().parents[1]
        source = 'VB-CABLE is already present. It will be reused. SoundCurrent restores your normal output when switched off or when you use %1.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' in installer.stem:
                self.assertNotIn('SCCablePresent', code)
            else:
                self.assertRegex(code, r'\$\{NSD_CreateLabel\}[^\n]+"\$\(SCCablePresent\)"')
                self.assertEqual(re.findall(r'^LangString SCCablePresent \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source.replace('%1','Quit app')])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            messages = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))
            self.assertTrue(catalog.finished(messages[source]), row['tag'])
            self.assertTrue(catalog.finished(messages['Quit app']), row['tag'])
            translated = messages[source].findtext('translation')
            catalog.validate_text(source, translated)
            for label in ('VB-CABLE','SoundCurrent'):
                with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                    catalog.validate_text(source, translated.replace(label, 'Other product'))
            with self.assertRaisesRegex(ValueError, 'Placeholder mismatch'):
                catalog.validate_text(source, translated.replace('%1','%2'))

    def test_setup_export_ignores_windows_default_codepage(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location('setup_export_codepage', root / 'scripts/windows_setup_catalogs.py')
        exporter = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(exporter)
        original = Path.read_text
        def windows_default(path, *args, **kwargs):
            if not args and 'encoding' not in kwargs:
                kwargs['encoding'] = 'cp1252'
            return original(path, *args, **kwargs)
        # The actual metadata contains multilingual display names that cp1252
        # cannot decode. Exercise real exporter/source checks under that default.
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'setup.json'
            broken = importlib.util.module_from_spec(spec)
            legacy = (root / 'scripts/windows_setup_catalogs.py').read_text(encoding='utf-8').replace(".read_text(encoding='utf-8')", '.read_text()')
            with patch.dict('sys.modules', {'localization': catalog}):
                exec(compile(legacy, '<legacy-export-fixture>', 'exec'), broken.__dict__)
            with patch.object(Path, 'read_text', windows_default):
                with self.assertRaises(UnicodeDecodeError):
                    broken.export(output)
                self.assertFalse(output.exists())
                exporter.export(output)
            payload = json.loads(output.read_text(encoding='utf-8'))
            self.assertEqual(len(payload['languages']), 34)
            self.assertIn('ar', payload['languages'])
            source = 'Audio driver setup'
            arabic = catalog.entries(catalog.DATA / 'soundcurrent_ar.ts')[source].findtext('translation')
            self.assertEqual(payload['languages']['ar'][source], arabic)

    def test_native_routing_notice_is_native_only_and_name_preserved(self):
        root = Path(__file__).resolve().parents[1]
        source = 'SoundCurrent Audio routes playback through the app. Choose your physical speakers or headphones inside the app. Their hardware drivers are preserved.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' not in installer.stem:
                self.assertNotIn('SCNativeRouting', code)
            else:
                self.assertRegex(code, r'\$\{NSD_CreateLabel\}[^\n]+"\$\(SCNativeRouting\)"')
                self.assertEqual(re.findall(r'^LangString SCNativeRouting \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            for name in ('Other driver', 'soundcurrent audio', 'SoundCurrent Audio SoundCurrent Audio'):
                with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                    catalog.validate_text(source, translated.replace('SoundCurrent Audio', name))

    def test_native_existing_driver_notice_preserves_names(self):
        root = Path(__file__).resolve().parents[1]
        source = 'SoundCurrent Audio is already present. With driver setup enabled, setup will register this app and keep the shared driver available for the other SoundCurrent app.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' not in installer.stem:
                self.assertNotIn('SCNativePresent', code)
            else:
                self.assertRegex(code, r'\$\{NSD_CreateLabel\}[^\n]+"\$\(SCNativePresent\)"')
                self.assertEqual(re.findall(r'^LangString SCNativePresent \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            for label in ('SoundCurrent Audio', 'SoundCurrent'):
                with self.assertRaisesRegex(ValueError, 'External installer label changed'):
                    catalog.validate_text(source, translated.replace(label, 'Other product'))

    def test_native_approval_notice_is_native_only_and_finished(self):
        root = Path(__file__).resolve().parents[1]
        source = 'Windows will request administrator approval for the signed driver manager. Setup will tell you if a restart is required.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' not in installer.stem:
                self.assertNotIn('SCNativeApproval', code)
            else:
                self.assertRegex(code, r'\$\{NSD_CreateLabel\}[^\n]+"\$\(SCNativeApproval\)"')
                self.assertEqual(re.findall(r'^LangString SCNativeApproval \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
            message = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[source]
            self.assertTrue(catalog.finished(message), row['tag'])
            catalog.validate_text(source, message.findtext('translation'))

    def test_native_shared_driver_notice_uses_running_app_wording(self):
        root = Path(__file__).resolve().parents[1]
        source = 'Quit any running SoundCurrent app before changing the shared driver. Removing one app keeps the driver if the other app still uses it.'
        for installer in sorted((root / 'packaging/windows').glob('*.nsi')):
            code = installer.read_text(encoding='utf-8')
            if 'native' not in installer.stem:
                self.assertNotIn('SCSharedDriverNotice', code)
            else:
                self.assertRegex(code, r'\$\{NSD_CreateLabel\}[^\n]+"\$\(SCSharedDriverNotice\)"')
                self.assertEqual(re.findall(r'^LangString SCSharedDriverNotice \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
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
            code = installer.read_text(encoding='utf-8')
            if 'native' in installer.stem:
                self.assertNotIn('SCCableRestart', code)
            else:
                self.assertRegex(code, r'\$\{NSD_CreateLabel\}[^\n]+"\$\(SCCableRestart\)"')
                self.assertEqual(re.findall(r'^LangString SCCableRestart \$\{LANG_ENGLISH\} "([^"]+)"$', code, re.M), [source])
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
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
        for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
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
                for ending in (b'\n', b'\r\n', None):
                    normalized = original.replace(b'\r\n', b'\n')
                    expected = normalized.replace(b'\n', ending) if ending else b''.join(
                        line[:-1] + (b'\r\n' if index % 2 else b'\n') if line.endswith(b'\n') else line
                        for index, line in enumerate(normalized.splitlines(keepends=True)))
                    fixture.write_bytes(expected)
                    exporter.generate_installer(fixture, generated, product, captions)
                    self.assertEqual(generated.read_bytes(), expected, 'Preserve LF/CRLF/mixed bytes on every host')
                    self.assertEqual(fixture.read_bytes(), expected)
                for replacement in ('', 'LangString SCConnectAudio ${LANG_ENGLISH} "One"\nLangString SCConnectAudio ${LANG_ENGLISH} "Two"'):
                    fixture.write_text(re.sub(r'^LangString SCConnectAudio.*$', replacement, source.read_text(encoding='utf-8'), flags=re.M), encoding='utf-8')
                    failed = folder / 'failed.nsi'
                    with self.assertRaisesRegex(ValueError, 'Missing or duplicate'):
                        exporter.generate_installer(fixture, failed, product, captions)
                    self.assertFalse(failed.exists())
                fixture.write_text(source.read_text(encoding='utf-8') + '\n!insertmacro MUI_LANGUAGE "French"\n', encoding='utf-8')
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
            result = json.loads(output.read_text(encoding='utf-8'))
            self.assertEqual(set(result['languages']), {row['tag'] for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8'))})
            self.assertFalse(result['installerLocaleActivationComplete'])
            for variants in result['languages'].values():
                self.assertEqual(set(variants), {'cable', 'native'})
                for routeName, route in variants.items():
                    self.assertEqual(set(route['captions']), {'SCConnectAudio', 'SCSetupAudio', 'SCInstallDriver', 'SCDriverCheckFailed', 'SCQuitBeforeUpdate', 'SCQuitBeforeUninstall', 'SCSetupRetryProgress', 'SCSetupFailedAppInstalled', 'SCWelcome'} | ({'SCCableRestart', 'SCCablePresent', 'SCCableRepair', 'SCCableSignedInstaller', 'SCCableSharedNotice', 'SCCableRouting', 'SCCableSetupProgress', 'SCCableRemovalFailed'} if routeName == 'cable' else {'SCSharedDriverNotice', 'SCNativeApproval', 'SCNativePresent', 'SCNativeRouting', 'SCNativeSetupProgress', 'SCNativeRemovalFailed'}))
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
            for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
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
            for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
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
            for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
                entry = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))[matches[0]]
                self.assertTrue(catalog.finished(entry), row['tag'])

    def test_declared_helper_source_is_extracted_without_seed(self):
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory)
            (data / 'seed-translations.json').write_bytes((catalog.DATA / 'seed-translations.json').read_bytes())
            # This fixture replaces helper declarations while retaining any
            # renderer declarations needed to audit the real source tree.
            cli_sources = catalog.DATA / 'cli-sources.json'
            if cli_sources.exists():
                (data / 'cli-sources.json').write_bytes(cli_sources.read_bytes())
            fixture = 'New owned helper diagnostic %1'
            self.assertNotIn(fixture, json.loads((data / 'seed-translations.json').read_text(encoding='utf-8'))['sources'])
            (data / 'setup-sources.json').write_text(json.dumps([fixture]), encoding='utf-8')
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
                (data / 'setup-sources.json').write_text(json.dumps(values), encoding='utf-8')
                with patch.object(catalog, 'DATA', data), self.assertRaisesRegex(ValueError, 'Invalid setup source inventory'):
                    catalog.setup_sources()

    def test_studio_owned_diagnostic_gaps_match_current_source(self):
        root = Path(__file__).resolve().parents[1]
        model = root / 'src/studio_model.cpp'
        if not model.exists():
            self.skipTest('Studio session model is not part of EQ')
        reasons = {catalog.literal(args[1]) for args in catalog.calls(model.read_text(encoding='utf-8'), 'require')
                   if len(args) == 2 and catalog.literal(args[1]) is not None}
        boundary = (root / 'src/audio_error_text.h').read_text(encoding='utf-8')
        mappings = dict(re.findall(r'if \(diagnostic == QStringLiteral\("([^"]+)"\)\)\s*return SC_TR\("([^"]+)"\);', boundary))
        gaps = json.loads((root / 'tests/results/localization/studio-model-diagnostic-gaps.json').read_text(encoding='utf-8'))
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
        profiles = json.loads((root / 'data/equipment/spinorama.json').read_text(encoding='utf-8'))
        keys = {profile['equipmentType'] for profile in profiles}
        header = (root / 'src/equipment_display_text.h').read_text(encoding='utf-8')
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
            for statement in [f'control->setStatusTip({wrapper}("Untranslated caption"));',
                              f'control->setWhatsThis({wrapper}("Untranslated caption"));',
                              f'menu->addAction({wrapper}("Untranslated caption"), callback);',
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

    def test_profile_authored_defaults_are_translated_at_creation(self):
        root = Path(__file__).resolve().parents[1]
        code = (root / 'src/equipment_profiles.cpp').read_text(encoding='utf-8')
        # Literal defaults authored here are human metadata, not imported data.
        raw = re.findall(r'\bp\.(?:brand|family|model|conditions|provenance)\s*=\s*("(?:\\.|[^"\\])*")', code)
        self.assertEqual(raw, [], 'Unmarked app-authored profile default')
        self.assertNotIn('"\\nUser-created profile"', code)
        self.assertNotIn('"\\nCustom copy of "', code)
        declared = set(catalog.sources())
        for source in ('Measured response', 'My equipment', 'New profile',
                       'User-created profile', 'Custom copy of %1', 'Imported %1; SHA256 %2'):
            self.assertIn(source, declared)

    def test_calibration_worker_display_prose_uses_declared_translations(self):
        root = Path(__file__).resolve().parents[1]
        code = (root / 'src/main.cpp').read_text(encoding='utf-8')
        block = code[code.index('int runCalibration('):code.index('struct CalibrationSuggestion')]
        statements = re.findall(r'QTextStream\(stderr\)([^;]*);', block)
        self.assertEqual(len(statements), 6)
        declared = set(catalog.sources())
        for statement in statements:
            marked = [catalog.literal(args[0]) for args in catalog.calls(statement, 'SC_TR')]
            literals = [catalog.literal(value) for value in re.findall(catalog.LITERAL, statement)]
            self.assertEqual(len(marked), 1)
            self.assertEqual(literals, marked, 'Untranslated calibration stderr fragment')
            self.assertIn(marked[0], declared)
        self.assertNotIn('calibrationStatus_->text().startsWith("Measurement failed")', code)

    def test_unmarked_owned_display_caption_is_rejected(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location('ui_audit', root / 'scripts/ui_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(audit)
        reviewed = json.loads((root / 'data/localization/ui-literal-exceptions.json').read_text(encoding='utf-8'))
        audit.check_reviewed_literals(root, reviewed)
        windows_rows = [dict(row, file=row['file'].replace('/', '\\')) for row in reviewed['exceptions']]
        with patch.object(audit, 'inventory', return_value={'candidates': windows_rows}):
            audit.check_reviewed_literals(root, reviewed)
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            (fixture / 'src').mkdir()
            (fixture / 'src/new.cpp').write_text('new QLabel("New untranslated caption");', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Unreviewed display literal'):
                audit.check_reviewed_literals(fixture, reviewed)

    def test_renderer_rejects_new_direct_untranslated_error(self):
        declared = {'Mapped error', 'Output already exists; select a new filename',
                    'Too many Studio channel filters'}
        catalog.check_renderer_diagnostics(
            'throw std::runtime_error("Mapped error"); '
            'throw std::runtime_error("Output already exists; choose a new filename"); '
            'throw std::runtime_error("Too many EQ bands for one channel");', declared)
        with self.assertRaisesRegex(ValueError, 'Undeclared renderer diagnostic'):
            catalog.check_renderer_diagnostics('throw std::runtime_error("New English error");', declared)

    def test_startup_diagnostics_reject_untranslated_production_logging(self):
        start = '    const auto runtime = QStandardPaths::writableLocation(0);'
        end = '    soundcurrent::ProcessingGuard processingGuard;'
        catalog.check_startup_diagnostics(start + 'qCritical("%s", qPrintable(SC_TR("Failure")));' + end)
        for unmarked in ['qCritical("English failure");',
                         'qCritical("%s", qPrintable(QStringLiteral("English failure")));']:
            with self.subTest(unmarked=unmarked), self.assertRaisesRegex(ValueError, 'Untranslated production startup'):
                catalog.check_startup_diagnostics(start + unmarked + end)

    def test_launcher_translation_preserves_identity_and_rejects_stale_text(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = root / 'data/localization'
            data.mkdir(parents=True)
            source = 'Launcher description'
            for tag, value in [('fr', 'Description française'), ('zh-Hans', '简体说明')]:
                tree = ET.Element('TS')
                message = ET.SubElement(ET.SubElement(tree, 'context'), 'message')
                ET.SubElement(message, 'source').text = source
                ET.SubElement(message, 'translation').text = value
                ET.ElementTree(tree).write(data / f'soundcurrent_{tag}.ts', encoding='utf-8')
            path = root / 'data/app.desktop'
            identity = ['[Desktop Entry]', 'Type=Application', 'Name=SoundCurrent',
                        'Exec=soundcurrent --background', 'Icon=stable.icon', 'Terminal=false']
            path.write_text('\n'.join(identity + ['Comment=' + source]) + '\n', encoding='utf-8')
            with patch.object(catalog, 'ROOT', root), patch.object(catalog, 'DATA', data):
                self.assertEqual(catalog.desktop_sources(), {source})
                catalog.maintain_desktop_comments(['en', 'fr', 'zh-Hans'], write=True)
                catalog.maintain_desktop_comments(['en', 'fr', 'zh-Hans'])
                lines = path.read_text(encoding='utf-8').splitlines()
                self.assertEqual([line for line in lines if not line.startswith('Comment')], identity)
                self.assertIn('Comment[fr]=Description française', lines)
                self.assertIn('Comment[zh_CN]=简体说明', lines)
                self.assertIn('Comment[zh_SG]=简体说明', lines)
                path.write_text(path.read_text(encoding='utf-8').replace('Description française', 'Wrong'), encoding='utf-8')
                with self.assertRaisesRegex(ValueError, 'Stale launcher translations'):
                    catalog.maintain_desktop_comments(['en', 'fr', 'zh-Hans'])
                with self.assertRaisesRegex(ValueError, 'Untranslated launcher description'):
                    catalog.desktop_comments(source, ['de'])

    def test_choice_list_detects_mixed_untranslated_entries(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location('ui_audit', root / 'scripts/ui_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(audit)
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            (fixture / 'src').mkdir()
            (fixture / 'src/panel.cpp').write_text(
                'box->addItems({SC_TR("Translated"), "Untranslated choice", '
                'QStringLiteral("Another choice"), SC_TR("Comma, translated")}); '
                'box->addItems(profileBrands);', encoding='utf-8')
            self.assertEqual([row['literal'] for row in audit.inventory(fixture)['candidates']],
                             ['Untranslated choice', 'Another choice'])
            with self.assertRaisesRegex(ValueError, 'Unreviewed display literal'):
                audit.check_reviewed_literals(fixture, {'exceptions': []})
            self.assertTrue(any(row['expression'] == 'profileBrands'
                                for row in audit.dynamic_inventory(fixture)['expressions']))

    def test_direct_backend_diagnostic_requires_declared_desktop_mapping(self):
        backend = 'throw std::runtime_error("Stable backend ID");'
        mapping = 'if (diagnostic == QStringLiteral("Stable backend ID")) return SC_TR("Displayed diagnostic");'
        catalog.check_backend_diagnostics(backend, mapping, {'Displayed diagnostic'})
        for broken, declared in (('', {'Displayed diagnostic'}), (mapping, set())):
            with self.assertRaisesRegex(ValueError, 'Unmapped direct Windows backend diagnostic'):
                catalog.check_backend_diagnostics(backend, broken, declared)
        catalog.check_backend_diagnostics('throw std::runtime_error(device + " external detail");', '', set())

    def test_studio_display_helpers_reject_raw_prose(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location('ui_audit', root / 'scripts/ui_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(audit)
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            (fixture / 'src').mkdir()
            (fixture / 'src/panel.cpp').write_text(
                'table({SC_TR("Translated"), "Raw header", "Hz"}); '
                'cell(view, 0, 1, QStringLiteral("Raw cell")); '
                'cell(view, 1, 1, QLocale().toString(value));', encoding='utf-8')
            self.assertEqual([row['literal'] for row in audit.inventory(fixture)['candidates']],
                             ['Raw header', 'Raw cell'])
            with self.assertRaisesRegex(ValueError, 'Unreviewed display literal'):
                audit.check_reviewed_literals(fixture, {'exceptions': []})
            self.assertTrue(any(row['expression'] == 'QLocale().toString(value)'
                                for row in audit.dynamic_inventory(fixture)['expressions']))

    def test_header_lists_detect_mixed_untranslated_entries(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location('ui_audit', root / 'scripts/ui_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(audit)
        for method in ('setHorizontalHeaderLabels', 'setVerticalHeaderLabels', 'setHeaderLabels'):
            with tempfile.TemporaryDirectory() as directory:
                fixture = Path(directory)
                (fixture / 'src').mkdir()
                (fixture / 'src/panel.cpp').write_text(
                    'view->' + method + '({SC_TR("Translated, header"), "Untranslated header", '
                    'QStringLiteral("Another header"), "Hz", "Q"}); '
                    'view->' + method + '(forwardedHeaders);', encoding='utf-8')
                self.assertEqual([row['literal'] for row in audit.inventory(fixture)['candidates']],
                                 ['Untranslated header', 'Another header'])
                with self.assertRaisesRegex(ValueError, 'Unreviewed display literal'):
                    audit.check_reviewed_literals(fixture, {'exceptions': []})
                self.assertTrue(any(row['expression'] == 'forwardedHeaders'
                                    for row in audit.dynamic_inventory(fixture)['expressions']))

    def test_dynamic_display_inventory_does_not_assume_translation_coverage(self):
        spec = importlib.util.spec_from_file_location('ui_audit', Path(__file__).resolve().parents[1] / 'scripts/ui_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(audit)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            (root / 'src/panel.cpp').write_text(
                'label->setText(SC_TR("Device: ") + userName); '
                'label->setText(QStringLiteral("English prefix: ") + SC_TR("Translated suffix")); '
                'label->setText(storedName); '
                'label->setText("Direct literal"); '
                'label->setAccessibleDescription(userDescription); '
                '// label->setText(commentOnly);', encoding='utf-8')
            result = audit.dynamic_inventory(root)
            rows = result['expressions']
            self.assertEqual(result['expressionCount'], 4)
            self.assertEqual(sum(row['containsTranslationCall'] for row in rows), 2)
            self.assertTrue(any('English prefix: ' in row['expression'] for row in rows))
            self.assertTrue(any(row['expression'] == 'storedName' for row in rows))
            self.assertFalse(result['wholeInterfaceCoverageProven'])
            self.assertFalse(result['nativeReviewed'])
            self.assertEqual([row['literal'] for row in audit.inventory(root)['candidates']], ['Direct literal'])

    def test_display_inventory_covers_tabs_and_help_captions(self):
        spec = importlib.util.spec_from_file_location('ui_audit', Path(__file__).resolve().parents[1] / 'scripts/ui_string_audit.py')
        audit = importlib.util.module_from_spec(spec)
        with patch.dict('sys.modules', {'localization': catalog}):
            spec.loader.exec_module(audit)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            (root / 'src/panel.cpp').write_text(
                'tabs->addTab(panel, "Audio tab"); '
                'tabs->insertTab(0, panel, QStringLiteral("Routing tab")); '
                'dialog.setInformativeText("Diagnostic detail"); '
                'QMessageBox::warning(parent, SC_TR("Translated title"), "Unmarked body"); '
                '\nQMessageBox named(QMessageBox::Warning, "Named title", QStringLiteral("Named body"));\n '
                'action->setStatusTip("Status guidance"); '
                'panel->setWhatsThis("Help guidance"); '
                'tabs->addTab(panel, SC_TR("Already translated")); '
                'panel->setWhatsThis(userDescription);', encoding='utf-8')
            rows = audit.inventory(root)['candidates']
            self.assertEqual({row['literal'] for row in rows}, {
                'Audio tab', 'Routing tab', 'Diagnostic detail',
                'Status guidance', 'Help guidance', 'Unmarked body', 'Named title', 'Named body'})

    def test_reviewed_wave_diagnostics_preserve_file_format_identity(self):
        for source, identifiers in catalog.REVIEWED_FILE_IDENTIFIERS.items():
            translated = 'Erreur du fichier : ' + source  # Retain repeated/overlapping identifiers.
            catalog.validate_text(source, translated)
            for identifier in identifiers:
                for damaged in (translated.replace(identifier, ''),
                                translated.replace(identifier, identifier.lower()),
                                translated + ' ' + identifier):
                    if damaged == translated:
                        continue  # Already-lowercase identifiers need other controls.
                    with self.assertRaisesRegex(ValueError, 'File-format identifier changed'):
                        catalog.validate_text(source, damaged)
        catalog.validate_text('Sound wave', 'Onde sonore')

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
        audit.check_backlog(audit.inventory(root), json.loads((root / 'data/localization/nsis-text-backlog.json').read_text(encoding='utf-8')))
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
        required = json.loads((root / 'data/localization/setup-sources.json').read_text(encoding='utf-8'))
        self.assertEqual(len(required), len(set(required)))
        calls = set()
        for file in ('cable-setup.ps1', 'native-audio-setup.ps1'):
            code = (root / 'packaging/windows' / file).read_text(encoding='utf-8')
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
            (data / 'catalogs.json').write_text(json.dumps([{'tag':'fr'}]), encoding='utf-8')
            (data / 'setup-sources.json').write_text(json.dumps(['First']), encoding='utf-8')
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
