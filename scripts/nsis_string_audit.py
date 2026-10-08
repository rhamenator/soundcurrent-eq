#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Inventory supported NSIS UI text sites; compiler/runtime qualification is separate."""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def logical_lines(code):
    """Remove comments outside quotes; retain NSIS dollar-escaped quotes verbatim."""
    block = False
    for number, line in enumerate(code.splitlines(), 1):
        out = []
        quoted = False
        i = 0
        while i < len(line):
            if block:
                if line[i:i+2] == '*/':
                    block = False
                    i += 2
                else:
                    i += 1
                continue
            if quoted and line[i:i+3] == '$\\"':
                out.append(line[i:i+3]); i += 3; continue
            if line[i] == '"':
                quoted = not quoted
            elif not quoted:
                if line[i:i+2] == '/*':
                    block = True; i += 2; continue
                if line[i] in ';#':
                    break
            out.append(line[i]); i += 1
        yield number, ''.join(out).strip()


def inventory(root):
    rows, languages, dynamic = [], {}, []
    quoted = re.compile(r'"((?:\$\\"|[^"])*)"')
    for path in sorted((root / 'packaging/windows').glob('*.nsi')):
        relative = path.relative_to(root).as_posix()
        languages[relative] = []
        for line, code in logical_lines(path.read_text(encoding='utf-8')):
            values = quoted.findall(code)
            if re.match(r'!insertmacro\s+MUI_LANGUAGE\b', code):
                languages[relative].extend(values)
                continue
            kind = None
            if re.match(r'\$\{NSD_Create(?:Label|Checkbox|Button|GroupBox|Link|RadioButton)\}', code):
                kind = 'control-caption'; selected = values[-1:]
            elif re.match(r'(?:MessageBox|DetailPrint|Section)\b', code):
                kind = code.split()[0]; selected = values[:1]
            elif re.match(r'!insertmacro\s+MUI_HEADER_TEXT\b', code):
                kind = 'page-heading'; selected = values[:2]
            elif re.match(r'!define\s+MUI_\w*TEXT\w*\b', code):
                kind = 'MUI-text'; selected = values[:1]
            if kind is None:
                continue
            if not selected:
                dynamic.append({'file': relative, 'line': line, 'kind': kind, 'expression': code})
            for literal in selected:
                marked = bool(re.fullmatch(r'\$\([A-Za-z_][A-Za-z_0-9]*\)', literal))
                rows.append({'file': relative, 'line': line, 'kind': kind, 'literal': literal,
                             'marked': marked, 'reviewStatus': 'language-string reference; qualification pending' if marked else 'untranslated candidate; contextual review pending'})
    return {'scope': 'Supported NSIS UI text sites; not a full NSIS parser',
            'wholeInterfaceCoverageProven': False, 'languages': languages,
            'candidates': rows, 'dynamicSites': dynamic}


def check_backlog(audit, backlog):
    key = lambda row: (row['file'], row['kind'], row['literal'])
    known = {key(row) for row in backlog['candidates'] if not row['marked']}
    approved = {key(row) for row in backlog['candidates'] if row['marked']}
    unreviewed = [row for row in audit['candidates'] if row['marked'] and key(row) not in approved]
    if unreviewed:
        raise ValueError('Unaudited installer language reference: ' + repr(unreviewed))
    new = [row for row in audit['candidates'] if not row['marked'] and key(row) not in known]
    if new:
        raise ValueError('New untranslated installer text: ' + repr(new))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--check-backlog', type=Path)
    args = parser.parse_args()
    result = inventory(args.root)
    if args.check_backlog:
        check_backlog(result, json.loads(args.check_backlog.read_text(encoding='utf-8')))
    text = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    else:
        print(text, end='')
