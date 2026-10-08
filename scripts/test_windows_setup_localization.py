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
