#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Export finished app translations for installed PowerShell audio helpers."""
import argparse
import json
from pathlib import Path
import localization as catalog

def export(destination):
    catalog.check(require_complete=True)
    required = set(json.loads((catalog.DATA / 'setup-sources.json').read_text()))
    languages = {}
    for row in json.loads((catalog.DATA / 'catalogs.json').read_text()):
        tag = row['tag']
        entries = catalog.entries(catalog.DATA / ('soundcurrent_' + tag + '.ts'))
        languages[tag] = {source: message.findtext('translation') for source, message in entries.items()
                          if catalog.finished(message)}
        missing = required - set(languages[tag])
        if missing:
            raise ValueError('Missing required setup translations for ' + tag + ': ' + ', '.join(sorted(missing)))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps({'schema': 1, 'languages': languages}, ensure_ascii=False), encoding='utf-8')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True, type=Path)
    export(parser.parse_args().output)
