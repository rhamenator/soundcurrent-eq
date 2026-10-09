#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inventory literal Qt display candidates, not a claim of whole-interface coverage.

Dynamic expressions, persisted names, backend diagnostics and installer text
require separate review. Candidates need semantic classification before marking.
"""
import argparse
import json
from pathlib import Path
import re
import localization as catalog

ROOT = Path(__file__).resolve().parents[1]
METHODS = {'setText':0, 'setToolTip':0, 'setAccessibleName':0,
           'setWindowTitle':0, 'setPlaceholderText':0, 'addItem':0,
           'addAction':0, 'addRow':0, 'addButton':0, 'setItemText':1,
           'addTab':1, 'insertTab':2, 'setInformativeText':0,
           'setStatusTip':0, 'setWhatsThis':0, 'setAccessibleDescription':0,
           'setTitle':0, 'setLabelText':0, 'setButtonText':1, 'setTabText':1,
           'getOpenFileName':1, 'getSaveFileName':1, 'getExistingDirectory':1,
           'showMessage':(0,1), 'setSuffix':0, 'setPrefix':0,
           'setHorizontalHeaderLabels':0, 'setVerticalHeaderLabels':0, 'addItems':0,
           'drawText':-1, 'getText':(1,2), 'getInt':(1,2), 'getDouble':(1,2),
           'information':(1,2), 'warning':(1,2), 'critical':(1,2), 'question':(1,2)}
CONSTRUCTORS = ('QLabel','QPushButton','QCheckBox','QGroupBox','QRadioButton',
                'QTableWidgetItem','QMenu','QAction')
# Brand identities and standard unit symbols are intentional display literals.
IDENTITIES = {'SoundCurrent EQ','SoundCurrent Studio','SoundCurrent','Q','Hz','dB','dBFS'}

def display_entries(expression, method):
    """Inspect explicit choice lists entry by entry; forwarded lists need provenance review."""
    expression = expression.strip()
    if method == 'addItems' and expression.startswith('{') and expression.endswith('}'):
        return next(catalog.calls('entries(' + expression[1:-1] + ')', 'entries'))
    return [expression]


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
            candidates[match.group(1)] = (1,2)
        for method,indices in candidates.items():
          for index in (indices if isinstance(indices, tuple) else (indices,)):
            for args in catalog.calls(code,method):
                if not args or len(args)<=index:
                    continue
                for expression in display_entries(args[index], method):
                    text = catalog.display_literal(expression)
                    if text is None or not text.strip() or text in IDENTITIES:
                        continue
                    records.append({'file':path.relative_to(root).as_posix(),
                                    'call':method,'argument':index,'literal':text})
    return {'scope':'Unmarked direct and exact Qt-wrapped literal display candidates; dynamic strings, stored/user data, backend and installer strings are not covered',
            'candidateCount':len(records),'candidates':records,
            'nativeReviewed':False,'wholeInterfaceCoverageProven':False}

def dynamic_inventory(root):
    """Report expressions for provenance review; translation calls are not proof.

    An expression may contain both translated and untranslated fragments, or
    forward external data. No automatic semantic classification is attempted.
    """
    records = []
    for path in sorted((root / 'src').rglob('*')):
        if path.suffix not in ('.cpp', '.h', '.hpp', '.inc'):
            continue
        code = path.read_text(encoding='utf-8')
        candidates = dict(METHODS)
        candidates.update({kind: 0 for kind in CONSTRUCTORS})
        for match in re.finditer(r'^\s*QMessageBox\s+(\w+)\s*\(', code, re.M):
            candidates[match.group(1)] = (1, 2)
        for method, indices in candidates.items():
            for index in (indices if isinstance(indices, tuple) else (indices,)):
                for args in catalog.calls(code, method):
                    if not args or len(args) <= index:
                        continue
                    expression = args[index].strip()
                    if not expression or catalog.display_literal(expression) is not None:
                        continue
                    records.append({'file': path.relative_to(root).as_posix(), 'call': method,
                                    'argument': index, 'expression': expression,
                                    'containsTranslationCall': bool(re.search(r'\b(?:SC_TR|text)\s*\(', expression))})
    return {'scope': 'Dynamic arguments to known Qt display sinks; expressions need manual provenance review. Contains-translation-call is informational and does not prove all fragments are localized.',
            'expressionCount': len(records), 'expressions': records,
            'nativeReviewed': False, 'wholeInterfaceCoverageProven': False}

def check_reviewed_literals(root, reviewed):
    """Reject newly unmarked captions; exemptions are exact source-site identities."""
    key = lambda row: (row['file'].replace('\\', '/'), row['call'], row['argument'], row['literal'])
    allowed = {key(row) for row in reviewed['exceptions']}
    for row in inventory(root)['candidates']:
        if key(row) not in allowed:
            raise ValueError('Unreviewed display literal: ' + str(row))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dynamic', action='store_true', help='Inventory dynamic display arguments for manual provenance review')
    parser.add_argument('--check-reviewed', action='store_true', help='Reject unmarked literals outside reviewed exact-site exemptions')
    args = parser.parse_args()
    if args.check_reviewed:
        check_reviewed_literals(ROOT, json.loads((ROOT / 'data/localization/ui-literal-exceptions.json').read_text(encoding='utf-8')))
    print(json.dumps(dynamic_inventory(ROOT) if args.dynamic else inventory(ROOT), ensure_ascii=False, indent=2))
