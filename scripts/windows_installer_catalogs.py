#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render reviewed installer captions from finished catalogs; language activation is separate."""
import argparse
import json
import re
from pathlib import Path
import localization as catalog

SOURCES = {
    'SCWelcomeQuit': 'Save your work and quit the running app before continuing. Closing its window keeps it running in the background.',
    'SCWelcomeInstall': 'Install or update %1. You do not need to uninstall an older version. Your settings, presets and equipment profiles will be kept.',
    'SCSetupFailedAppInstalled': '%1 setup did not finish. %2 itself is installed. Use %3 in the Start menu to retry; see setup details for the reason.',
    'SCCableRemovalFailed': 'VB-CABLE removal did not finish. This app was kept so you can retry.',
    'SCNativeRemovalFailed': 'Shared audio driver removal did not finish. This app was kept so you can retry. Quit any running SoundCurrent app, then retry uninstalling.',
    'SCSetupRetryProgress': '%1 setup did not finish. Retry using the Start menu shortcut.',
    'SCCableSetupProgress': 'Opening %1 setup...',
    'SCNativeSetupProgress': 'Setting up the shared %1 driver...',
    'SCQuitBeforeUninstall': 'Quit %1 before uninstalling it.',
    'SCQuitBeforeUpdate': 'Quit %1 before updating. Closing the window keeps it running. No uninstall is needed.',
    'SCCableRouting': 'VB-CABLE routes playback through the app. Choose speakers inside SoundCurrent. VB-CABLE is VB-Audio donationware: https://vb-cable.com — donations are welcome.',
    'SCCableSharedNotice': 'Quit any running equalizer before driver setup. When removing the last SoundCurrent app, its uninstaller offers VB-CABLE removal. Other software may also need the cable. Extra A/B cables are not bundled.',
    'SCCableSignedInstaller': 'Setup opens VB-Audio’s signed installer. Click Install Driver, then restart Windows before using the equalizer or VB-CABLE settings.',
    'SCCableRepair': 'VB-CABLE has a driver record but no usable audio endpoints. Setup offers repair: remove the driver, restart, reinstall, and restart again.',
    'SCCablePresent': 'VB-CABLE is already present. It will be reused. SoundCurrent restores your normal output when switched off or when you use %1.',
    'SCQuitAction': 'Quit app',
    'SCNativeRouting': 'SoundCurrent Audio routes playback through the app. Choose your physical speakers or headphones inside the app. Their hardware drivers are preserved.',
    'SCNativePresent': 'SoundCurrent Audio is already present. With driver setup enabled, setup will register this app and keep the shared driver available for the other SoundCurrent app.',
    'SCNativeApproval': 'Windows will request administrator approval for the signed driver manager. Setup will tell you if a restart is required.',
    'SCSharedDriverNotice': 'Quit any running SoundCurrent app before changing the shared driver. Removing one app keeps the driver if the other app still uses it.',
    'SCCableRestart': 'VB-CABLE setup requires a Windows restart. Restart before using the equalizer or opening VB-CABLE settings.',
    'SCDriverCheckFailed': 'Setup could not check the driver. You can retry with %1 in the app or Start menu.',
    'SCSetupAction': 'Audio driver setup',
    'SCConnectAudio': 'Connect your audio',
    'SCSetupAudio': 'Set up %1 for %2.',
    'SCInstallCable': 'Install VB-CABLE if missing (administrator approval)',
    'SCInstallNative': 'Install or update the shared SoundCurrent Audio driver',
}


def validate_language_map(nsis_language_directory=None):
    """Validate explicit locale/Windows identities; never guess from tag spelling."""
    payload = json.loads((catalog.DATA / 'installer-language-map.json').read_text(encoding='utf-8'))
    if payload.get('schema') != 1:
        raise ValueError('Unsupported installer language map schema')
    rows = payload['languages']
    tags = [row['tag'] for row in rows]
    expected = [row['tag'] for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8'))]
    if len(tags) != len(set(tags)) or set(tags) != set(expected):
        raise ValueError('Installer map must cover every catalog exactly once')
    for field in ('nsisLanguage', 'windowsLanguageId'):
        if len({row[field] for row in rows}) != len(rows):
            raise ValueError('Duplicate installer language identity: ' + field)
    missing = []
    for row in rows:
        if not isinstance(row['windowsLanguageId'], int) or not 0 < row['windowsLanguageId'] <= 65535:
            raise ValueError('Invalid Windows language ID')
        if row['rtl'] != (row['tag'] in ('ar', 'he', 'fa')):
            raise ValueError('Installer direction mismatch: ' + row['tag'])
        if nsis_language_directory is None:
            continue
        path = Path(nsis_language_directory) / (row['nsisLanguage'] + '.nlf')
        if not path.exists():
            missing.append(row['tag'])
            if row['builtinAssetsExpected']:
                raise ValueError('Missing expected NSIS language asset: ' + str(path))
            continue
        fields = [line for line in path.read_text(encoding='utf-8-sig').splitlines()
                  if line and not line.startswith('#')]
        if fields[0] != 'NLF v6' or int(fields[1]) != row['windowsLanguageId'] or (fields[5] == 'RTL') != row['rtl']:
            raise ValueError('NSIS language asset identity mismatch: ' + row['tag'])
        if not path.with_suffix('.nsh').exists():
            raise ValueError('Missing MUI language strings: ' + row['tag'])
    return {'languages': rows, 'missingBuiltinAssets': missing,
            'installerLocaleActivationComplete': False}

def format_names(template, driver, product):
    if sorted(catalog.PLACEHOLDER.findall(template)) != ['%1', '%2']:
        raise ValueError('Installer subtitle needs exactly one %1 and %2')
    return re.sub(r'%[12]', lambda match: driver if match[0] == '%1' else product, template)

def format_value(template, value):
    if catalog.PLACEHOLDER.findall(template) != ['%1']:
        raise ValueError('Installer message needs exactly one %1')
    return template.replace('%1', value)

def format_values(template, values):
    expected = ['%' + str(index + 1) for index in range(len(values))]
    if sorted(catalog.PLACEHOLDER.findall(template)) != sorted(expected):
        raise ValueError('Installer message parameter mismatch')
    return re.sub(r'%([1-9][0-9]*)', lambda match: values[int(match[1]) - 1], template)

def nsis_escape(text):
    # Escape dollar first: inserted NSIS escape sequences must remain active.
    if any(ord(character) < 32 and character not in '\r\n\t' for character in text):
        raise ValueError('Unsupported control character in installer caption')
    return text.replace('$', '$$').replace('"', '$\\"').replace('\r', '$\\r').replace('\n', '$\\n').replace('\t', '$\\t')

def export(destination, product):
    if product not in ('SoundCurrent EQ', 'SoundCurrent Studio'):
        raise ValueError('Unknown installer product')
    catalog.check(require_complete=True)
    validate_language_map()
    languages = {}
    for row in json.loads((catalog.DATA / 'catalogs.json').read_text(encoding='utf-8')):
        messages = catalog.entries(catalog.DATA / ('soundcurrent_' + row['tag'] + '.ts'))
        translations = {}
        for key, source in SOURCES.items():
            message = messages.get(source)
            if message is None or not catalog.finished(message):
                raise ValueError('Missing finished installer caption: ' + row['tag'] + '/' + key)
            translated = message.findtext('translation')
            catalog.validate_text(source, translated)
            translations[key] = translated
        variants = {}
        for route, driver, checkbox in [('cable', 'VB-CABLE', 'SCInstallCable'),
                                         ('native', 'SoundCurrent Audio', 'SCInstallNative')]:
            captions = {'SCWelcome': format_value(translations['SCWelcomeInstall'], product) + '\r\n\r\n' + translations['SCWelcomeQuit'],
                        'SCSetupFailedAppInstalled': format_values(translations['SCSetupFailedAppInstalled'], (driver, product, 'Audio driver setup')),
                        'SCSetupRetryProgress': format_value(translations['SCSetupRetryProgress'], driver),
                        'SCQuitBeforeUpdate': format_value(translations['SCQuitBeforeUpdate'], product),
                        'SCQuitBeforeUninstall': format_value(translations['SCQuitBeforeUninstall'], product),
                        'SCConnectAudio': translations['SCConnectAudio'],
                        'SCSetupAudio': format_names(translations['SCSetupAudio'], driver, product),
                        'SCInstallDriver': translations[checkbox],
                        'SCDriverCheckFailed': format_value(translations['SCDriverCheckFailed'], translations['SCSetupAction'])}
            if route == 'native':
                captions['SCNativeRemovalFailed'] = translations['SCNativeRemovalFailed']
                captions['SCNativeSetupProgress'] = format_value(translations['SCNativeSetupProgress'], driver)
                captions['SCNativeRouting'] = translations['SCNativeRouting']
                captions['SCNativePresent'] = translations['SCNativePresent']
                captions['SCNativeApproval'] = translations['SCNativeApproval']
                captions['SCSharedDriverNotice'] = translations['SCSharedDriverNotice']
            if route == 'cable':
                captions['SCCableRemovalFailed'] = translations['SCCableRemovalFailed']
                captions['SCCableSetupProgress'] = format_value(translations['SCCableSetupProgress'], driver)
                captions['SCCableRouting'] = translations['SCCableRouting']
                captions['SCCableSharedNotice'] = translations['SCCableSharedNotice']
                captions['SCCableSignedInstaller'] = translations['SCCableSignedInstaller']
                captions['SCCableRepair'] = translations['SCCableRepair']
                captions['SCCablePresent'] = format_value(translations['SCCablePresent'], translations['SCQuitAction'])
                captions['SCCableRestart'] = translations['SCCableRestart']
            variants[route] = {'captions': captions, 'nsisEscaped': {key: nsis_escape(text) for key, text in captions.items()}}
        languages[row['tag']] = variants
    result = {'schema': 1, 'product': product, 'scope': 'Reviewed heading, subtitle, driver checkbox and driver-check guidance and cable restart notice and shared-driver and administrator-approval and existing-driver and native-routing and existing-cable and incomplete-driver repair and signed-installer and shared-cable and cable-routing donation and quit-before-update/uninstall and setup progress and native and cable removal failure and installed-app setup failure and welcome guidance only',
              'installerLocaleActivationComplete': False, 'nativeSpeakerVerified': False,
              'sources': SOURCES, 'languages': languages}
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return result

def generate_installer(source, destination, product, captions):
    """Write a build copy; current installer language inventory is still English."""
    if captions.get('schema') != 1 or captions.get('product') != product:
        raise ValueError('Installer caption payload does not match product/schema')
    if source.resolve() == destination.resolve():
        raise ValueError('Generated installer must not overwrite its source')
    stem = 'soundcurrent-eq' if product == 'SoundCurrent EQ' else 'soundcurrent-studio'
    if product not in ('SoundCurrent EQ', 'SoundCurrent Studio') or source.name not in (stem + '.nsi', stem + '-native.nsi'):
        raise ValueError('Installer source does not match product')
    route = 'native' if source.name.endswith('-native.nsi') else 'cable'
    # Byte I/O preserves LF, CRLF and mixed endings independently of host OS.
    text = source.read_bytes().decode('utf-8')
    declared = re.findall(r'^!insertmacro MUI_LANGUAGE "([^"]+)"\r?$', text, re.M)
    if declared != ['English']:
        raise ValueError('Installer language activation needs an explicit expanded generation plan')
    for key, value in captions['languages']['en'][route]['nsisEscaped'].items():
        pattern = r'^LangString ' + re.escape(key) + r' \$\{LANG_ENGLISH\} "(?:\$\\"|[^"])*"(\r?)$'
        replacement = 'LangString ' + key + ' ${LANG_ENGLISH} "' + value + '"'
        text, count = re.subn(pattern, lambda match: replacement + match[1], text, flags=re.M)
        if count != 1:
            raise ValueError('Missing or duplicate installer caption definition: ' + key)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(text.encode('utf-8'))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--product', choices=['SoundCurrent EQ', 'SoundCurrent Studio'], required=True)
    parser.add_argument('--installer-source', type=Path)
    parser.add_argument('--installer-output', type=Path)
    arguments = parser.parse_args()
    if bool(arguments.installer_source) != bool(arguments.installer_output):
        parser.error('--installer-source and --installer-output must be used together')
    captions = export(arguments.output, arguments.product)
    if arguments.installer_source:
        generate_installer(arguments.installer_source, arguments.installer_output, arguments.product, captions)
