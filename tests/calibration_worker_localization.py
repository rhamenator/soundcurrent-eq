# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the actual calibration subprocess without device lookup or playback."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    catalogs = root / 'data/localization'
    tags = [item['tag'] for item in json.loads((catalogs / 'catalogs.json').read_text(encoding='utf-8'))]
    choices = [(tag, tag) for tag in tags] + [('fr_CA', 'fr'), ('DE-de', 'de'), ('xx-Unknown', 'en')]
    with tempfile.TemporaryDirectory(prefix='soundcurrent-worker-i18n-') as directory:
        for requested, resolved in choices:
            messages = {m.findtext('source'): m.findtext('translation') for m in ET.parse(catalogs / f'soundcurrent_{resolved}.ts').findall('./context/message')}
            expected = messages['Measurement failed: %1'].replace('%1', messages['Test level is outside the allowed range']) + '\n'
            env = os.environ.copy()
            env.update({'SOUNDCURRENT_WORKER_LANGUAGE': requested, 'SOUNDCURRENT_WORKER_FORMAT_LOCALE': 'ar-EG',
                        'XDG_CONFIG_HOME': directory, 'APPDATA': directory, 'LOCALAPPDATA': directory})
            # Both levels fail the first range check, before device enumeration,
            # temporary audio files, microphone capture or speaker playback.
            for level in ('-55', '-4'):
                result = subprocess.run([str(args.executable.resolve()), '--calibration-worker', 'unused-output', 'unused-input', level, 'sweep'],
                                        env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
                assert result.returncode == 1, (requested, level, result.returncode, result.stderr)
                assert result.stdout == b'', (requested, 'unexpected machine output')
                assert result.stderr.decode('utf-8').splitlines() == expected.splitlines(), (requested, level, result.stderr)
        assert not list(Path(directory).rglob('*.wav')), 'Invalid-level fixture produced audio files'
    print(f'PASS: actual core-only calibration worker, {len(tags)} catalogs plus regional/fallback cases, exact UTF-8 failure text, no device lookup or playback')


if __name__ == '__main__':
    main()
