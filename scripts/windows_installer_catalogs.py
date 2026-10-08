#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render reviewed installer captions from finished catalogs; language activation is separate."""
import argparse
import json
import re
from pathlib import Path
import localization as catalog

SOURCES = {
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
                        'SCInstallDriver': translations[checkbox]}
            variants[route] = {'captions': captions, 'nsisEscaped': {key: nsis_escape(text) for key, text in captions.items()}}
        languages[row['tag']] = variants
    result = {'schema': 1, 'product': product, 'scope': 'Reviewed heading, subtitle and driver checkbox captions only',
              'installerLocaleActivationComplete': False, 'nativeSpeakerVerified': False,
              'sources': SOURCES, 'languages': languages}
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--product', choices=['SoundCurrent EQ', 'SoundCurrent Studio'], required=True)
    arguments = parser.parse_args()
    export(arguments.output, arguments.product)
