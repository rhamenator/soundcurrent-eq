#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Embed data-only translations in the standalone Bash installer, without eval."""
import argparse
import json
from pathlib import Path
import re
import shlex
import localization as catalog

ROOT = Path(__file__).resolve().parents[1]
BEGIN = '# BEGIN GENERATED INSTALLER CATALOG'
END = '# END GENERATED INSTALLER CATALOG'

def payload(require_complete=False):
    data = json.loads((ROOT / 'data/localization/linux-installer.json').read_text(encoding='utf-8'))
    if data.get('schema') != 1 or data.get('nativeReviewed') is not False:
        raise ValueError('Invalid Linux installer catalog metadata')
    sources = data['sources']
    if not sources or any(not re.fullmatch(r'[a-z][a-z0-9_]*', key) for key in sources):
        raise ValueError('Invalid installer source identifier')
    if data['languages'].get('en') != sources:
        raise ValueError('English installer catalog must match source definitions')
    supported = {row['tag'] for row in json.loads((ROOT / 'data/localization/catalogs.json').read_text())}
    if not set(data['languages']).issubset(supported):
        raise ValueError('Unknown installer language')
    for language, entries in data['languages'].items():
        if set(entries) != set(sources):
            raise ValueError('Incomplete installer language: ' + language)
        for key, value in entries.items():
            if not isinstance(value, str) or '\x00' in value:
                raise ValueError('Invalid installer translation')
            catalog.validate_text(sources[key], value)
            if re.search(r'%[1-9][0-9]|%L|%n', value):
                raise ValueError('Installer supports only %1 through %9 placeholders')
            for token in ('--language', '--dry-run', '--download-only', '--package-file',
                          'TAG', 'PATH', 'bash', 'x86-64', 'RHEL', '10', 'Fedora', '44',
                          'sha256sum', 'SHA256', 'curl', 'wget', 'glibc', '[y/N]'):
                if token in sources[key] and value.count(token) != sources[key].count(token):
                    raise ValueError('Installer command/version token changed: ' + language + ': ' + key + ': ' + token)
        quit_message = catalog.entries(ROOT / 'data/localization' / f'soundcurrent_{language}.ts').get('Quit app')
        if quit_message is None or not catalog.finished(quit_message):
            raise ValueError('Missing app quit caption: ' + language)
        caption = quit_message.findtext('translation')
        if any(caption not in entries[key] for key in ('quit', 'installed')):
            raise ValueError('Installer instruction does not name app quit caption: ' + language)
    missing = sorted(supported - set(data['languages']))
    if require_complete and missing:
        raise ValueError('Installer locales not yet translated: ' + ', '.join(missing))
    return data, missing

def block(data):
    lines = [BEGIN, '# Generated from data/localization/linux-installer.json; do not edit this block.',
             'sc_resolve_language() {', '    local sc_requested=$1 sc_tag sc_base',
             '    local -a sc_candidates', '    IFS=: read -r -a sc_candidates <<< "$sc_requested"',
             '    for sc_tag in "${sc_candidates[@]}"; do',
             '        sc_tag=${sc_tag%%.*}; sc_tag=${sc_tag%%@*}',
             "        sc_tag=$(printf '%s' \"$sc_tag\" | LC_ALL=C tr 'A-Z_' 'a-z-')",
             '        case "$sc_tag" in zh|zh-cn|zh-sg) sc_tag=zh-hans;; zh-tw|zh-hk|zh-mo) sc_tag=zh-hant;; esac',
             '        for sc_base in "$sc_tag" "${sc_tag%%-*}"; do', '            case "$sc_base" in']
    for language in data['languages']:
        lines.append('                ' + language.lower() + ') printf \'%s\' ' + shlex.quote(language) + '; return;;')
    lines.extend(['            esac', '        done', '    done', "    printf '%s' en", '}', 'sc_lookup() {', '    case "${sc_language:-en}:$1" in'])
    for language, entries in data['languages'].items():
        for key, value in entries.items():
            lines.append('        ' + language + ':' + key + ") printf '%s' " + shlex.quote(value) + ';;')
    for key,value in data['sources'].items():
        lines.append('        *:' + key + ") printf '%s' " + shlex.quote(value) + ';;')
    lines.extend(['        *) return 1;;', '    esac', '}', END])
    return '\n'.join(lines)

def maintain(write=False, require_complete=False):
    data, missing = payload(require_complete)
    path = ROOT / 'scripts/install-linux.run'
    text = path.read_text(encoding='utf-8')
    expected = block(data)
    pattern = re.compile(re.escape(BEGIN) + r'[\s\S]*?' + re.escape(END))
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        raise ValueError('Installer must have exactly one catalog block')
    actual = matches[0].group()
    if write:
        path.write_text(text[:matches[0].start()] + expected + text[matches[0].end():], encoding='utf-8')
    elif actual != expected:
        raise ValueError('Stale embedded installer catalog; run --update')
    calls = set(re.findall(r'\b(?:sc_text|sc_error)\s+[\'"]?([a-z][a-z0-9_]*)', text[ matches[0].end():]))
    body = text[matches[0].end():]
    for match in re.finditer(r"\b(?:echo|message|confirm)\s+(['\"])(.*?)\1", body):
        argument = match.group(2)
        if argument != '$1' and '$(sc_text ' not in argument:
            raise ValueError('New untranslated Linux installer caption: ' + argument)
    if calls != set(data['sources']):
        raise ValueError('Installer text calls differ from declared sources: ' + str(calls ^ set(data['sources'])))
    print(f'PASS: {len(data["sources"])} installer messages, {len(data["languages"])} populated locales; {len(missing)} locales pending; native verification unverified')

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--update', action='store_true')
    parser.add_argument('--require-complete', action='store_true')
    args=parser.parse_args()
    maintain(args.update,args.require_complete)
