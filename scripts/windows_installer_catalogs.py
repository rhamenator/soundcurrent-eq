#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render reviewed installer captions from finished catalogs; language activation is separate."""
import argparse
import json
import re
from pathlib import Path
import localization as catalog

SOURCES = {
    'SCSharedDriverNotice': 'Quit any running SoundCurrent app before changing the shared driver. Removing one app keeps the driver if the other app still uses it.',
    'SCCableRestart': 'VB-CABLE setup requires a Windows restart. Restart before using the equalizer or opening VB-CABLE settings.',
    'SCDriverCheckFailed': 'Setup could not check the driver. You can retry with %1 in the app or Start menu.',
    'SCSetupAction': 'Audio driver setup',
    'SCConnectAudio': 'Connect your audio',
    'SCSetupAudio': 'Set up %1 for %2.',
    'SCInstallCable': 'Install VB-CABLE if missing (administrator approval)',
    'SCInstallNative': 'Install or update the shared SoundCurrent Audio driver',
}

def format_names(template, driver, product):
    if sorted(catalog.PLACEHOLDER.findall(template)) != ['%1', '%2']:
        raise ValueError('Installer subtitle needs exactly one %1 and %2')
    return re.sub(r'%[12]', lambda match: driver if match[0] == '%1' else product, template)

def nsis_escape(text):
    # Escape dollar first: inserted NSIS escape sequences must remain active.
    if any(ord(character) < 32 and character not in '\r\n\t' for character in text):
        raise ValueError('Unsupported control character in installer caption')
    return text.replace('$', '$$').replace('"', '$\\"').replace('\r', '$\\r').replace('\n', '$\\n').replace('\t', '$\\t')

def export(destination, product):
    if product not in ('SoundCurrent EQ', 'SoundCurrent Studio'):
        raise ValueError('Unknown installer product')
    catalog.check(require_complete=True)
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
            captions = {'SCConnectAudio': translations['SCConnectAudio'],
                        'SCSetupAudio': format_names(translations['SCSetupAudio'], driver, product),
                        'SCInstallDriver': translations[checkbox],
                        'SCDriverCheckFailed': translations['SCDriverCheckFailed'].replace('%1', translations['SCSetupAction'])}
            if route == 'native':
                captions['SCSharedDriverNotice'] = translations['SCSharedDriverNotice']
            if route == 'cable':
                captions['SCCableRestart'] = translations['SCCableRestart']
            variants[route] = {'captions': captions, 'nsisEscaped': {key: nsis_escape(text) for key, text in captions.items()}}
        languages[row['tag']] = variants
    result = {'schema': 1, 'product': product, 'scope': 'Reviewed heading, subtitle, driver checkbox and driver-check guidance and cable restart notice and shared-driver guidance only',
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
    text = source.read_text(encoding='utf-8')
    declared = re.findall(r'^!insertmacro MUI_LANGUAGE "([^"]+)"$', text, re.M)
    if declared != ['English']:
        raise ValueError('Installer language activation needs an explicit expanded generation plan')
    for key, value in captions['languages']['en'][route]['nsisEscaped'].items():
        pattern = r'^LangString ' + re.escape(key) + r' \$\{LANG_ENGLISH\} "(?:\$\\"|[^"])*"$'
        replacement = 'LangString ' + key + ' ${LANG_ENGLISH} "' + value + '"'
        text, count = re.subn(pattern, lambda match: replacement, text, flags=re.M)
        if count != 1:
            raise ValueError('Missing or duplicate installer caption definition: ' + key)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(text, encoding='utf-8')

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
