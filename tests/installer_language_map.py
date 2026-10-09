#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Validate real NSIS asset identities and malformed-map rejection."""
import sys
from pathlib import Path
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import windows_installer_catalogs as installer

class InstallerLanguageMap(unittest.TestCase):
    def test_catalog_coverage(self):
        result=installer.validate_language_map()
        self.assertEqual(len(result['languages']),34)
        self.assertFalse(result['installerLocaleActivationComplete'])

    def test_real_nsis_assets(self):
        directory=Path('/usr/share/nsis/Contrib/Language files')
        if not directory.is_dir():
            self.skipTest('NSIS asset directory unavailable on this host')
        result=installer.validate_language_map(directory)
        self.assertEqual(result['missingBuiltinAssets'],['sw'])

    def test_mutations_rejected(self):
        import json
        data=installer.catalog.DATA
        original=json.loads((data/'installer-language-map.json').read_text())
        for field,value in [('tag','en'),('windowsLanguageId',1033),('rtl',True)]:
            mutated=json.loads(json.dumps(original))
            mutated['languages'][1][field]=value
            real_loads=json.loads
            def loads(text, **kwargs):
                return mutated if 'builtinAssetsExpected' in text else real_loads(text, **kwargs)
            with self.subTest(field=field),patch.object(installer.json,'loads',side_effect=loads):
                with self.assertRaises(ValueError):
                    installer.validate_language_map()

if __name__=='__main__': unittest.main()
