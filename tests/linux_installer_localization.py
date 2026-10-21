#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise standalone installer text without downloads, installation or elevation."""
import importlib.util
import json
import os
import pty
import select
import time
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import linux_installer_catalogs as catalogs
SCRIPT = ROOT / 'scripts/install-linux.run'

class InstallerTests(unittest.TestCase):
    def run_bash(self, code, values=(), environment=None):
        env = dict(os.environ, SC_INSTALLER_LANGUAGE='en', DISPLAY='', WAYLAND_DISPLAY='')
        if environment:
            env.update(environment)
        return subprocess.run(['bash','-c',code,'--',str(SCRIPT),*values],
                              env=env,capture_output=True,text=True,timeout=10)

    def test_embedded_catalog_and_complete_release_gate(self):
        catalogs.maintain()
        data, missing = catalogs.payload()
        if missing:
            with self.assertRaisesRegex(ValueError, 'Installer locales not yet translated'):
                catalogs.payload(require_complete=True)
        else:
            catalogs.payload(require_complete=True)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'scripts').mkdir()
            file = root / 'scripts/install-linux.run'
            file.write_text(SCRIPT.read_text() + "\nmessage 'New English caption'\n",encoding='utf-8')
            # Retain production catalog inputs; mutate only the copied script.
            (root / 'data').symlink_to(ROOT / 'data', target_is_directory=True)
            with patch.object(catalogs,'ROOT',root), self.assertRaisesRegex(ValueError,'New untranslated Linux installer caption'):
                catalogs.maintain()

    def test_complete_release_gate_rejects_removed_locale(self):
        data, _ = catalogs.payload()
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            target = fixture / 'data/localization'
            target.mkdir(parents=True)
            (target / 'catalogs.json').write_bytes((ROOT / 'data/localization/catalogs.json').read_bytes())
            del data['languages']['fr']
            for language in data['languages']:
                (target / f'soundcurrent_{language}.ts').symlink_to(ROOT / 'data/localization' / f'soundcurrent_{language}.ts')
            (target / 'linux-installer.json').write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8')
            with patch.object(catalogs,'ROOT',fixture), self.assertRaisesRegex(ValueError,'Installer locales not yet translated'):
                catalogs.payload(require_complete=True)

    def test_quit_instruction_must_name_the_actual_app_caption(self):
        data, _ = catalogs.payload()
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            target = fixture / 'data/localization'
            target.mkdir(parents=True)
            (target / 'catalogs.json').write_bytes((ROOT / 'data/localization/catalogs.json').read_bytes())
            for language in data['languages']:
                (target / f'soundcurrent_{language}.ts').symlink_to(ROOT / 'data/localization' / f'soundcurrent_{language}.ts')
            # Keep intent, placeholders and command tokens; remove only the full
            # displayed action caption to verify the new correspondence guard.
            data['languages']['fr']['quit'] = data['languages']['fr']['quit'].replace('«Quitter l’application»', 'Quitter')
            (target / 'linux-installer.json').write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8')
            with patch.object(catalogs,'ROOT',fixture), self.assertRaisesRegex(ValueError,'does not name app quit caption'):
                catalogs.payload()

    def test_lookup_and_single_pass_opaque_substitutions(self):
        data, _ = catalogs.payload()
        with tempfile.TemporaryDirectory() as directory:
            sentinel = Path(directory) / 'must-not-exist'
            opaque = '%1 / $(touch ' + str(sentinel) + ') / `touch ' + str(sentinel) + '` / $HOME / 音声'
            for language, entries in data['languages'].items():
                result=self.run_bash('source "$1"; sc_language=$2; sc_text saved "$3"',
                                     (language,opaque))
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(result.stdout,entries['saved'].replace('%1',opaque))
                self.assertFalse(sentinel.exists())
                for key,value in entries.items():
                    result=self.run_bash('source "$1"; sc_language=$2; sc_lookup "$3"',
                                         (language,key))
                    self.assertEqual(result.returncode,0,result.stderr)
                    self.assertEqual(result.stdout,value)

    def test_help_errors_locale_chain_and_fallback(self):
        data, _ = catalogs.payload()
        for language,entries in data['languages'].items():
            result=self.run_bash('bash "$1" --language "$2" --help',(language,))
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(result.stdout.strip(),entries['usage'].replace('%1',str(SCRIPT)))
            result=self.run_bash('bash "$1" --unknown',environment={'SC_INSTALLER_LANGUAGE':language})
            self.assertEqual(result.returncode,2)
            self.assertEqual(result.stderr.strip(),entries['unknown_option'])
            result=self.run_bash('source "$1"; select_package arch "" rolling',environment={'SC_INSTALLER_LANGUAGE':language})
            self.assertEqual(result.returncode,1)
            self.assertEqual(result.stderr.strip(),entries['unsupported'])
        result=self.run_bash('source "$1"; printf "%s" "$sc_language"',environment={'SC_INSTALLER_LANGUAGE':'xx:fr_FR.UTF-8'})
        self.assertEqual(result.stdout,'fr')
        result=self.run_bash('source "$1"; printf "%s" "$sc_language"',environment={'SC_INSTALLER_LANGUAGE':'$(touch /invalid)/unknown'})
        self.assertEqual(result.stdout,'en')

    def test_real_terminal_confirmation_accept_and_cancel(self):
        data, _ = catalogs.payload()
        for language, entries in data['languages'].items():
            for answer, expected in (('n', 1), ('Y', 0)):
                master, slave = pty.openpty()
                env = dict(os.environ, SC_INSTALLER_LANGUAGE=language,
                           DISPLAY='', WAYLAND_DISPLAY='')
                child = subprocess.Popen(
                    ['bash', '-c', 'source "$1"; confirm "$(sc_text download_confirm Demo 1 Ubuntu)"',
                     '--', str(SCRIPT)], stdin=slave, stdout=slave, stderr=slave, env=env)
                os.close(slave)
                captured = bytearray()
                try:
                    prompt = entries['continue'].encode('utf-8')
                    deadline = time.monotonic() + 5
                    while prompt not in captured:
                        if time.monotonic() >= deadline:
                            self.fail(f'{language}: terminal prompt timed out')
                        if select.select([master], [], [], 0.1)[0]:
                            captured.extend(os.read(master, 65536))
                    os.write(master, (answer + '\n').encode('ascii'))
                    self.assertEqual(child.wait(timeout=5), expected, language)
                    body = entries['download_confirm'].replace('%1','Demo').replace('%2','1').replace('%3','Ubuntu')
                    self.assertIn(body.encode('utf-8'), captured, language)
                finally:
                    if child.poll() is None:
                        child.kill()
                        child.wait()
                    os.close(master)

    def test_dialog_titles_bodies_and_cancel_are_forwarded(self):
        data, _=catalogs.payload()
        with tempfile.TemporaryDirectory() as directory:
            fake=Path(directory)/'zenity'
            fake.write_text('#!'+sys.executable+'\nimport json,os,sys\nopen(os.environ["SC_CAPTURE_FILE"],"w").write(json.dumps(sys.argv[1:]))\nsys.exit(int(os.environ.get("SC_FAKE_RESULT","0")))\n')
            fake.chmod(0o755)
            capture=Path(directory)/'capture.json'
            for language in data['languages']:
                environment={'PATH':directory+os.pathsep+os.environ['PATH'],'DISPLAY':':fixture',
                             'SC_CAPTURE_FILE':str(capture),'SC_INSTALLER_LANGUAGE':language}
                result=self.run_bash('source "$1"; message "$(sc_text verification)"',environment=environment)
                self.assertEqual(result.returncode,0,result.stderr)
                argv=json.loads(capture.read_text())
                self.assertIn('--title='+data['languages'][language]['installer_title'],argv)
                self.assertIn('--text='+data['languages'][language]['verification'],argv)
                environment['SC_FAKE_RESULT']='1'
                result=self.run_bash('source "$1"; confirm "$(sc_text download_confirm A 1 Ubuntu)"',environment=environment)
                self.assertEqual(result.returncode,1)
                argv=json.loads(capture.read_text())
                self.assertIn('--title='+data['languages'][language]['install_title'],argv)
                self.assertIn('--text='+data['languages'][language]['download_confirm'].replace('%1','A').replace('%2','1').replace('%3','Ubuntu'),argv)

if __name__=='__main__':
    unittest.main()
