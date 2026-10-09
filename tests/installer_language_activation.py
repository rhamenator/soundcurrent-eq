#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compile actual multilingual installers using inert payloads; never execute them."""
import json
import io
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import windows_installer_catalogs as installer
PRODUCT='SoundCurrent Studio' if (ROOT/'src/studio_model.cpp').exists() else 'SoundCurrent EQ'
STEM='soundcurrent-studio' if PRODUCT.endswith('Studio') else 'soundcurrent-eq'

class Activation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary=tempfile.TemporaryDirectory(prefix='installer-activation-')
        cls.folder=Path(cls.temporary.name)
        cls.captions=installer.export(cls.folder/'captions.json',PRODUCT)

    @classmethod
    def tearDownClass(cls): cls.temporary.cleanup()

    def test_generated_routing_and_persistence(self):
        rows=installer.validate_language_map()['languages']
        for source in sorted((ROOT/'packaging/windows').glob('*.nsi')):
            target=self.folder/source.name
            original=source.read_bytes()
            installer.generate_installer(source,target,PRODUCT,self.captions,activate=True)
            text=target.read_text(encoding='utf-8')
            route='native' if source.name.endswith('-native.nsi') else 'cable'
            self.assertEqual(source.read_bytes(),original)
            self.assertEqual(len(re.findall(r'^!insertmacro MUI_LANGUAGE(?:EX)? ',text,re.M)),34)
            for row in rows:
                for key in self.captions['languages'][row['tag']][route]['captions']:
                    identity='${LANG_ENGLISH}' if row['tag']=='en' else str(row['windowsLanguageId'])
                    self.assertEqual(len(re.findall(r'^LangString '+key+' '+re.escape(identity)+' ',text,re.M)),1)
                self.assertEqual(text.count('StrCpy $SCLocaleTag "'+row['tag']+'"'),4 if row['tag']=='en' else 2)
            self.assertIn('!insertmacro MUI_LANGDLL_DISPLAY',text)
            self.assertIn('!define MUI_LANGDLL_ALWAYSSHOW',text)
            self.assertIn('!define MUI_LANGDLL_ALLLANGUAGES',text)
            self.assertIn('!define MUI_LANGDLL_WINDOWTITLE "$(SCInstallerLanguageTitle)"',text)
            self.assertEqual(len(re.findall(r'^LangString SCInstallerLanguageTitle ',text,re.M)),34)
            self.assertIn('!insertmacro MUI_UNGETLANGUAGE',text)
            self.assertIn('"InstallerLocale" "$SCLocaleTag"',text)
            self.assertIn('nsDialogs::SetRTL $(^RTL)',text)
            helpers=[line for line in text.splitlines() if '-File "' in line and ('audio-setup.ps1' in line or 'cable-setup.ps1' in line)]
            self.assertGreaterEqual(len(helpers),4)
            for line in helpers: self.assertIn('-Language "$SCLocaleTag"',line)
            self.assertIn('StrCpy $InstallDriver 0 ; Silent app updates never install/elevate a driver.',text)

    def test_generated_text_under_cp1252_default(self):
        # Model a Windows default independently of this host's UTF-8 mode.
        original_open=Path.open
        def legacy_open(path,mode='r',buffering=-1,encoding=None,errors=None,newline=None):
            if 'b' not in mode and encoding in (None,'locale'):
                encoding='cp1252'
            return original_open(path,mode,buffering,encoding,errors,newline)
        original_text_encoding=io.text_encoding
        def legacy_text_encoding(encoding,stacklevel=2):
            return 'cp1252' if encoding is None else original_text_encoding(encoding,stacklevel)
        with patch.object(Path,'open',legacy_open),patch.object(io,'text_encoding',legacy_text_encoding):
            self.test_generated_routing_and_persistence()
            self.test_shortcut_names_and_exact_cleanup()
            # Negative control: omitting UTF-8 must reproduce the Windows decode fault.
            with self.assertRaises(UnicodeDecodeError):
                (self.folder/(STEM+'.nsi')).read_text()

    def test_shortcut_names_and_exact_cleanup(self):
        for value in ('../other','folder/name','folder\\name','bad:name','NUL','CON.txt','COM1','LPT9','trailing.',' trailing','bad\nname',''):
            with self.subTest(value=value), self.assertRaises(ValueError): installer.validate_shortcut_name(value)
        for source in sorted((ROOT/'packaging/windows').glob('*.nsi')):
            generated=self.folder/('shortcuts-'+source.name)
            installer.generate_installer(source,generated,PRODUCT,self.captions,activate=True)
            text=generated.read_text(encoding='utf-8')
            route='native' if source.name.endswith('-native.nsi') else 'cable'
            names={'Uninstall','Audio driver setup','Install VB-CABLE','VB-CABLE settings'}
            for tag,variants in self.captions['languages'].items():
                pack=variants[route]
                names.update(pack['shortcutCaptions'].values())
                if tag!='en':
                    self.assertIn(installer.nsis_escape(pack['localizedSetupFailure']),text)
            for name in names:
                instruction='  Delete "$SMPROGRAMS\\'+PRODUCT+'\\'+installer.nsis_escape(name)+'.lnk"'
                self.assertEqual(text.count(instruction),2,name)
            self.assertNotIn('Delete "$SMPROGRAMS\\'+PRODUCT+'\\*',text)
            for key in ('SCShortcutUninstall','SCShortcutSetup'):
                self.assertIn('CreateShortcut "$SMPROGRAMS\\'+PRODUCT+'\\$('+key+').lnk"',text)
            if route=='cable': self.assertIn('CreateShortcut "$SMPROGRAMS\\'+PRODUCT+'\\$(SCShortcutCableSettings).lnk"',text)
            self.assertIn('CreateShortcut "$DESKTOP\\'+PRODUCT+'.lnk"',text)

    def test_activation_checkout_line_endings(self):
        for source in sorted((ROOT/'packaging/windows').glob('*.nsi')):
            original=source.read_bytes().replace(b'\r\n',b'\n')
            outputs=[]
            for variant in ('lf','crlf','mixed'):
                folder=self.folder/variant;folder.mkdir(exist_ok=True)
                fixture=folder/source.name
                data=original if variant=='lf' else original.replace(b'\n',b'\r\n')
                if variant=='mixed':
                    data=b''.join(line[:-1]+(b'\r\n' if index%2 else b'\n') if line.endswith(b'\n') else line for index,line in enumerate(original.splitlines(keepends=True)))
                fixture.write_bytes(data)
                generated=folder/('generated-'+source.name)
                installer.generate_installer(fixture,generated,PRODUCT,self.captions,activate=True)
                outputs.append(generated.read_bytes())
                self.assertEqual(fixture.read_bytes(),data)
                self.assertNotIn(b'\r\n',outputs[-1])
            self.assertEqual(outputs[0],outputs[1])
            self.assertEqual(outputs[0],outputs[2])

    def test_compile_real_templates(self):
        compiler=shutil.which('makensis')
        if not compiler: self.skipTest('NSIS compiler unavailable')
        stage=self.folder/'stage';stage.mkdir(exist_ok=True)
        names=[STEM+'.exe','setup-localization.ps1','setup-translations.json','soundcurrent-cable-setup-guard.exe','soundcurrent-driver-manager.exe','soundcurrent-route-guardian.exe','Qt6Core.dll','msvcp140.dll','vcruntime140.dll','concrt140.dll','soundcurrentvad.inf','soundcurrentvad.sys','soundcurrentvad.cat','cable.zip']
        for name in names: (stage/name).write_bytes(b'Inert compile fixture, never execute')
        manifest=self.folder/'uninstall.nsh';manifest.write_text('; Inert compile fixture\n',encoding='utf-8')
        for source in sorted((ROOT/'packaging/windows').glob('*.nsi')):
            generated=self.folder/('compiled-'+source.name)
            installer.generate_installer(source,generated,PRODUCT,self.captions,activate=True)
            defines={'APP_EXE':stage/(STEM+'.exe'),'OUTPUT':self.folder/(source.stem+'.exe'),'SOURCE_ROOT':ROOT,'DLL_DIR':stage,'UNINSTALL_PAYLOAD':manifest,'CABLE_ZIP':stage/'cable.zip','DRIVER_DIR':stage}
            prefix='/' if sys.platform=='win32' else '-'
            args=[compiler,prefix+'INPUTCHARSET','UTF8',prefix+'V2']+[prefix+'D'+key+'='+str(value) for key,value in defines.items()]+[str(generated)]
            result=subprocess.run(args,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertNotIn('not set in language',result.stdout+result.stderr)
            self.assertTrue(defines['OUTPUT'].is_file())

if __name__=='__main__': unittest.main()
