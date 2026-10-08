#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inventory literal Qt display candidates, not a claim of whole-interface coverage.

Dynamic expressions, persisted names, backend diagnostics and installer text
require separate review. Candidates need semantic classification before marking.
"""
import json
from pathlib import Path
import re
import localization as catalog

ROOT = Path(__file__).resolve().parents[1]
METHODS = {'setText':0, 'setToolTip':0, 'setAccessibleName':0,
           'setWindowTitle':0, 'setPlaceholderText':0, 'addItem':0,
           'addAction':0, 'addRow':0, 'addButton':0, 'setItemText':1,
           'getOpenFileName':1, 'getSaveFileName':1,
           'information':1, 'warning':1, 'critical':1, 'question':1}
CONSTRUCTORS = ('QLabel','QPushButton','QCheckBox','QGroupBox','QRadioButton',
                'QTableWidgetItem')
# Brand identities and standard unit symbols are intentional display literals.
IDENTITIES = {'SoundCurrent EQ','SoundCurrent Studio','SoundCurrent','Q','Hz','dB','dBFS'}

def inventory(root):
    records = []
    for path in sorted((root/'src').rglob('*')):
        if path.suffix not in ('.cpp','.h','.hpp','.inc'):
            continue
        code = path.read_text(encoding='utf-8')
        candidates = dict(METHODS)
        candidates.update({kind:0 for kind in CONSTRUCTORS})
        # Named QMessageBox declarations have icon, title and body arguments.
        for match in re.finditer(r'^\s*QMessageBox\s+(\w+)\s*\(',code,re.M):
            candidates[match.group(1)] = 1
        for method,index in candidates.items():
            for args in catalog.calls(code,method):
                if len(args)<=index:
                    continue
                text = catalog.literal(args[index])
                if text is None or not text.strip() or text in IDENTITIES:
                    continue
                records.append({'file':str(path.relative_to(root)),
                                'call':method,'argument':index,'literal':text})
    return {'scope':'Unmarked direct literal Qt display candidates; dynamic strings, stored/user data, backend and installer strings are not covered',
            'candidateCount':len(records),'candidates':records,
            'nativeReviewed':False,'wholeInterfaceCoverageProven':False}

if __name__ == '__main__':
    print(json.dumps(inventory(ROOT),ensure_ascii=False,indent=2))
