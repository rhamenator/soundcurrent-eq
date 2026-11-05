#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Rebuild admitted corrections from pinned numerical measurement inputs; no network or audio.
Build soundcurrent-profile-compiler first. Source acquisition, hashes and exclusions are
recorded separately in data/equipment/published-collection-report.json.
"""
import argparse,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',type=Path,default=ROOT/'build/soundcurrent-profile-compiler');p.add_argument('--check',action='store_true');args=p.parse_args()
 source=ROOT/'data/equipment/published-response-inputs.json';dest=ROOT/'data/equipment/published.json';reportpath=ROOT/'data/equipment/published-collection-report.json'
 with tempfile.TemporaryDirectory(prefix='soundcurrent-equipment-') as temporary:
  output=Path(temporary)/'admitted.json'
  subprocess.run([str(args.compiler.resolve()),str(source),str(output)],check=True,timeout=120)
  result=json.loads(output.read_text());admitted=sorted(result['accepted'],key=lambda p:(p['kind'],p['brand'].casefold(),p['family'].casefold(),p['model'].casefold()))
  report=json.loads(reportpath.read_text());assert report['accepted']==len(admitted) and report['rejected']==result['rejected'],'Admission report differs; review changed inputs or fitter explicitly.'
  if args.check:assert admitted==json.loads(dest.read_text()),'Published corrections differ from deterministic compiler output.'
  else:dest.write_text(json.dumps(admitted,ensure_ascii=False,indent=2)+'\n')
 print(f'PASS: {len(admitted)} admitted, measured corrections; no placeholders.')
if __name__=='__main__':main()
