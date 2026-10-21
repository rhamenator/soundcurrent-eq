#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inventory localization candidates; findings require human reachability review.

This is a discovery aid, not a complete UI coverage or linguistic verifier.
It never changes sources, catalogs or processing settings.
"""
import argparse
import importlib.util
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path


def inventory(root):
    spec = importlib.util.spec_from_file_location('catalog_tools', root / 'scripts/localization.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    known = {m.findtext('source') for m in ET.parse(root / 'data/localization/soundcurrent_en.ts').findall('.//message')}
    findings = []
    # Calls may span lines. Balanced parsing skips comments and nested expressions.
    methods = {'runtime_error': 0, 'invalid_argument': 0, 'showError': 0,
               'setText': 0, 'setStatusTip': 0, 'setToolTip': 0,
               'setWindowTitle': 0, 'setAccessibleName': 0,
               'setPlaceholderText': 0, 'QLabel': 0, 'QPushButton': 0,
               'QCheckBox': 0, 'addTab': 1, 'addRow': 0}
    for p in sorted((root / 'src').rglob('*')):
        if p.suffix not in ('.cpp', '.h', '.hpp', '.inc'):
            continue
        code = p.read_text(encoding='utf-8')
        for method, index in methods.items():
            for args in module.calls(code, method):
                if len(args) <= index:
                    continue
                value = module.literal(args[index])
                if not value or not re.search(r'[A-Za-z]{3}', value):
                    continue
                # May be invariant identifiers/units, developer-only errors or
                # strings translated by a view boundary; classify manually.
                findings.append({'file':str(p.relative_to(root)), 'call':method,
                                 'source':value, 'inCatalog':value in known,
                                 'classification':'needs reachability and runtime translation review'})
    for p in sorted((root / 'packaging').rglob('*.nsi')):
        for line, text in enumerate(p.read_text(encoding='utf-8').splitlines(), 1):
            if re.search(r'\b(MessageBox|DetailPrint|Section|CreateShortCut)\b', text) and '"' in text:
                findings.append({'file':str(p.relative_to(root)), 'line':line,
                                 'call':'NSIS candidate', 'sourceLine':text.strip(),
                                 'classification':'installer UI, path or identifier; review required'})
    for p in sorted((root / 'scripts').rglob('*.py')):
        if p.name in ('localization.py', 'localization_inventory.py'):
            continue
        for line, text in enumerate(p.read_text(encoding='utf-8').splitlines(), 1):
            if re.search(r'\b(print|raise|sys\.exit)\s*\(', text):
                findings.append({'file':str(p.relative_to(root)), 'line':line,
                                 'call':'Python diagnostic candidate', 'sourceLine':text.strip(),
                                 'classification':'helper or developer diagnostic; review required'})
    return {'scope':'Literal C++ exceptions and selected view calls across src, NSIS UI candidates, Python script diagnostics. Dynamic/composed strings, shell/PowerShell messages, native dialogs and data-driven names require further review. Not whole-application coverage.',
            'catalogSourceCount':len(known), 'candidateCount':len(findings), 'candidates':findings}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    result = inventory(root)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"Recorded {result['candidateCount']} candidates; manual classification required.")
