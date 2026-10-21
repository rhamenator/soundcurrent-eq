#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Swahili NSIS base structure/token and compile qualification, no installer execution."""
import collections
import json
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'packaging/windows/languages'
TOKEN=re.compile(r'\$\([^)]*\)|\$_[A-Z]+|\$[0-9]+|%s|\\[rn]')

def read_nlf(path):
    lines=path.read_text(encoding='utf-8-sig').splitlines()
    fields=[line for line in lines if line and not line.startswith('#')]
    messages={}
    for index,line in enumerate(lines):
        if line.startswith('# ^'):
            key=line[2:]
            if key in messages: raise ValueError('Duplicate NLF key')
            messages[key]=lines[index+1]
    return fields[:6],messages

class SwahiliBase(unittest.TestCase):
    def test_structure_and_tokens(self):
        header,messages=read_nlf(ASSETS/'Swahili.nlf')
        self.assertEqual(header,['NLF v6','1089','-','-','-','-'])
        review=json.loads((ASSETS/'swahili-base-review.json').read_text(encoding='utf-8'))
        self.assertFalse(review['nativeSpeakerVerified'])
        self.assertEqual(len(messages),89)
        self.assertEqual(set(messages),{row['id'] for row in review['messages']})
        for row in review['messages']:
            with self.subTest(id=row['id']):
                actual=messages[row['id']]
                self.assertEqual(actual,row['translation'])
                self.assertEqual(collections.Counter(TOKEN.findall(actual)),collections.Counter(TOKEN.findall(row['source'])))
                self.assertEqual(actual.startswith('"'),row['source'].startswith('"'))
                self.assertEqual(actual.endswith('"'),row['source'].endswith('"'))
                if row['source'].startswith('"'):
                    self.assertEqual(actual.endswith(' "'),row['source'].endswith(' "'))

    def test_mui_tokens(self):
        rows=re.findall(r'\$\{LangFileString\} (\w+) "(.*)"',(ASSETS/'Swahili.nsh').read_text(encoding='utf-8-sig'))
        review=json.loads((ASSETS/'swahili-mui-review.json').read_text(encoding='utf-8'))
        self.assertFalse(review['nativeSpeakerVerified'])
        self.assertEqual(len(rows),62)
        self.assertEqual(rows,[(row['id'],row['translation']) for row in review['messages']])
        for row in review['messages']:
            with self.subTest(id=row['id'],source=row['source']):
                self.assertEqual(collections.Counter(TOKEN.findall(row['source'])),collections.Counter(TOKEN.findall(row['translation'])))
                self.assertNotEqual(row['source'],row['translation'])

    def test_real_nsis_compile(self):
        compiler=shutil.which('makensis')
        if not compiler: self.skipTest('NSIS compiler unavailable')
        with tempfile.TemporaryDirectory(prefix='swahili-nlf-') as directory:
            folder=Path(directory)
            shutil.copyfile(ASSETS/'Swahili.nlf',folder/'Swahili.nlf')
            shutil.copyfile(ASSETS/'Swahili.nsh',folder/'Swahili.nsh')
            (folder/'fixture.nsi').write_text('Unicode true\nName "SoundCurrent locale fixture"\nOutFile "fixture.exe"\nLoadLanguageFile "Swahili.nlf"\nPage license\nLicenseData "Swahili.nlf"\nPage directory\nPage instfiles\nSection\nDetailPrint "$(^SetupCaption)"\nSectionEnd\n',encoding='utf-8')
            result=subprocess.run([compiler,'-V2','fixture.nsi'],cwd=folder,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertTrue((folder/'fixture.exe').is_file())
            mui='Unicode true\n!include "MUI2.nsh"\nName "SoundCurrent locale fixture"\nOutFile "mui.exe"\nInstallDir "$TEMP\\SoundCurrentLocaleFixture"\n!insertmacro MUI_PAGE_WELCOME\n!insertmacro MUI_PAGE_LICENSE "Swahili.nlf"\n!insertmacro MUI_PAGE_COMPONENTS\n!insertmacro MUI_PAGE_DIRECTORY\n!insertmacro MUI_PAGE_INSTFILES\n!insertmacro MUI_PAGE_FINISH\n!insertmacro MUI_UNPAGE_WELCOME\n!insertmacro MUI_UNPAGE_LICENSE "Swahili.nlf"\n!insertmacro MUI_UNPAGE_COMPONENTS\n!insertmacro MUI_UNPAGE_CONFIRM\n!insertmacro MUI_UNPAGE_DIRECTORY\n!insertmacro MUI_UNPAGE_INSTFILES\n!insertmacro MUI_UNPAGE_FINISH\n!insertmacro MUI_LANGUAGEEX "." "Swahili"\nSection "Fixture"\nWriteUninstaller "$INSTDIR\\uninstall.exe"\nSectionEnd\nSection "Uninstall"\nSectionEnd\n'
            for alternative in (False,True):
                source=('!define NSIS_CONFIG_COMPONENTPAGE_ALTERNATIVE\n' if alternative else '')+mui
                (folder/'mui.nsi').write_text(source,encoding='utf-8')
                result=subprocess.run([compiler,'-V2','mui.nsi'],cwd=folder,capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                self.assertNotIn('missing',result.stdout.lower()+result.stderr.lower())
                self.assertTrue((folder/'mui.exe').is_file())
            # Compile only. The fixture is never installed or executed.

if __name__=='__main__': unittest.main()
