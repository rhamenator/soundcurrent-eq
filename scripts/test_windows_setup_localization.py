#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run data-only setup localization checks without loading a driver helper."""
from pathlib import Path
import subprocess
import tempfile
import windows_setup_catalogs

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='soundcurrent-setup-i18n-') as directory:
    catalog = Path(directory) / 'setup-translations.json'
    windows_setup_catalogs.export(catalog)
    subprocess.run(['pwsh', '-NoProfile', '-NonInteractive', '-File',
                    str(root / 'tests/windows_setup_localization.ps1'),
                    '-CatalogPath', str(catalog)], check=True)
