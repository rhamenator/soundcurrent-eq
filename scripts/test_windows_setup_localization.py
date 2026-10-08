#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run catalog checks and safe no-action helper faults without driver operations."""
from pathlib import Path
import subprocess
import json
import shutil
import tempfile
import windows_setup_catalogs

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='soundcurrent-setup-i18n-') as directory:
    catalog = Path(directory) / 'setup-translations.json'
    windows_setup_catalogs.export(catalog)
    subprocess.run(['pwsh', '-NoProfile', '-NonInteractive', '-File',
                    str(root / 'tests/windows_setup_localization.ps1'),
                    '-CatalogPath', str(catalog)], check=True)

    # No action switches: the real helper rejects before any driver/PnP check.
    # Quiet mode also avoids loading Windows Forms on this host.
    shutil.copyfile(root / 'packaging/windows/setup-localization.ps1', Path(directory) / 'setup-localization.ps1')
    data = json.loads(catalog.read_text())
    for helper in ('cable-setup.ps1', 'native-audio-setup.ps1'):
        script = Path(directory) / helper
        shutil.copyfile(root / 'packaging/windows' / helper, script)
        for language in ('fr','ar','nn'):
            result = subprocess.run(['pwsh','-NoProfile','-NonInteractive','-ExecutionPolicy','RemoteSigned',
                                     '-File',str(script),'-Quiet','-Language',language], capture_output=True)
            expected = data['languages'][language]['Choose one audio setup action.']
            if helper == 'native-audio-setup.ps1':
                expected = data['languages'][language]['Audio driver setup did not finish: %1'].replace('%1', expected)
            if result.returncode != 30 or result.stdout.decode('utf-8').strip() != expected:
                raise RuntimeError('Actual quiet helper validation failed: ' + helper + ': ' + language)
    print('PASS: actual cable/native quiet action-validation errors in fr/ar/nn; no action switches or driver/endpoint checks')

    # Mutation check: the actual guard must reject new raw UI text, not merely list it.
    negative_root = Path(directory) / 'negative-prose-fixture'
    for relative in ('tests/windows_setup_localization.ps1', 'scripts/windows_setup_inventory.ps1',
                     'packaging/windows/setup-localization.ps1', 'packaging/windows/cable-setup.ps1',
                     'packaging/windows/native-audio-setup.ps1', 'data/localization/setup-sources.json',
                     'data/localization/setup-prose-backlog.json'):
        destination = negative_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / relative, destination)
    with (negative_root / 'packaging/windows/cable-setup.ps1').open('a') as fixture:
        fixture.write("\nNotice 'Unexpected helper caption'\n")
    rejected = subprocess.run(['pwsh', '-NoProfile', '-NonInteractive', '-File',
                               str(negative_root / 'tests/windows_setup_localization.ps1'),
                               '-CatalogPath', str(catalog)], capture_output=True)
    if rejected.returncode == 0 or 'New untranslated helper prose:' not in rejected.stderr.decode('utf-8'):
        raise RuntimeError('New raw helper UI message was not rejected by the regression gate')
    print('PASS: mutation test rejected newly added untranslated helper caption')
