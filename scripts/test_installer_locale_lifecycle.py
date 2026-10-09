#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inert checks only; never pass -Run to the system-changing lifecycle harness."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import windows_installer_catalogs
powershell=shutil.which('pwsh') or shutil.which('powershell')
if not powershell:
    print('SKIP: PowerShell unavailable; Windows installer lifecycle fixture not executed')
    sys.exit(77)
root=Path(__file__).resolve().parents[1]
product='Studio' if (root/'src/studio_model.cpp').exists() else 'EQ'
with tempfile.TemporaryDirectory(prefix='installer-locale-fixture-') as temporary:
    directory=Path(temporary)
    catalog=directory/'captions.json'
    windows_installer_catalogs.export(catalog,'SoundCurrent '+product)
    mapping=root/'data/localization/installer-language-map.json'
    subprocess.run([powershell,'-NoProfile','-NonInteractive','-File',str(root/'tests/windows_installer_locale_fixture.ps1'),'-CatalogPath',str(catalog),'-LanguageMapPath',str(mapping)],check=True)
    result=subprocess.run([powershell,'-NoProfile','-NonInteractive','-File',str(root/'tests/windows_installer_locale_lifecycle.ps1'),'-Product',product,'-InstallerPath','missing-installer.exe','-CatalogPath',str(catalog),'-LanguageMapPath',str(mapping),'-ResultDirectory',str(directory/'results')],capture_output=True,text=True)
    if result.returncode!=0 or 'Use -Run only' not in result.stdout or (directory/'results').exists():
        raise RuntimeError('Lifecycle no-Run guard failed')
    print('PASS: omitted -Run returns before lifecycle mutations')
