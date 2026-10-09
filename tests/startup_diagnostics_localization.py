#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the real Linux startup lock error without reaching audio setup."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
executable = Path(sys.argv[1]).resolve()
source = '%1 is already running or its instance lock is unavailable'
product = 'SoundCurrent EQ' if executable.name == 'soundcurrent-eq' else 'SoundCurrent Studio'
for language in ('fr', 'ar', 'nn'):
    tree = ET.parse(root / 'data/localization' / f'soundcurrent_{language}.ts')
    template = next(m.findtext('translation') for m in tree.findall('./context/message')
                    if m.findtext('source') == source)
    expected = template.replace('%1', product)
    with tempfile.TemporaryDirectory(prefix='soundcurrent-startup-i18n-') as directory:
        runtime = Path(directory) / 'runtime'
        runtime.mkdir(mode=0o700)
        # A directory at the lock-file pathname causes an immediate lock error.
        # No existing process, global settings or system audio route is touched.
        (runtime / (executable.name + '.lock')).mkdir()
        config = Path(directory) / 'config'
        config.mkdir()
        environment = dict(os.environ, QT_QPA_PLATFORM='offscreen',
                           XDG_RUNTIME_DIR=str(runtime), XDG_CONFIG_HOME=str(config))
        result = subprocess.run([str(executable), '--language', language],
                                env=environment, capture_output=True, timeout=10)
        stderr = result.stderr.decode('utf-8')
        if result.returncode != 1 or expected not in stderr:
            raise RuntimeError(f'Startup diagnostic failed for {language}: code={result.returncode}, stderr={stderr!r}')
        if any(config.rglob('*.conf')):
            raise RuntimeError('Startup error unexpectedly wrote application settings')
        print(f'PASS: {language} translated startup lock diagnostic; exit 1; isolated runtime/config; no audio startup')
