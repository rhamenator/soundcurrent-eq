#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Assemble release companions from already-built and tested packages."""
import hashlib, pathlib, re, subprocess, sys
import linux_installer_catalogs
linux_installer_catalogs.maintain(require_complete=True)
root=pathlib.Path(__file__).resolve().parents[1]
out=pathlib.Path(sys.argv[1]).resolve()
project=re.search(r'project\((soundcurrent-[a-z]+) VERSION ([0-9.]+)',(root/'CMakeLists.txt').read_text())
name,version=project.groups(); label='EQ' if name.endswith('-eq') else 'Studio'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def find(pattern):
 matches=list(out.rglob(pattern))
 if len(matches)!=1:raise SystemExit(f'Expected one {pattern}, found {len(matches)}')
 return matches[0]
installer=(root/'scripts/install-linux.run').read_text()
installer=re.sub(r"^version='[^']+'",f"version='{version}'",installer,flags=re.M)
installer=re.sub(r"^release='[^']+'",f"release='v{version}'",installer,flags=re.M)
for key,pattern in [('deb_hash',f'{name}_{version}_amd64.deb'),('fedora_hash',f'{name}-{version}-1.fc44.x86_64.rpm'),('rhel_hash',f'{name}-{version}-1.el10.x86_64.rpm')]:
 installer=re.sub(rf"^{key}='[^']+'",f"{key}='{sha(find(pattern))}'",installer,flags=re.M)
p=out/f'SoundCurrent-{label}-Linux-Installer.run';p.write_text(installer);p.chmod(0o755)
subprocess.run(['bash','-n',str(p)],check=True)
subprocess.run(['git','archive','--format=tar.gz',f'--prefix={name}-{version}/',f'--output={out/name}-{version}-source.tar.gz','HEAD'],cwd=root,check=True)
for source,dest in [('docs/linux-installer.md','Linux-installation.md'),('docs/windows-cable-interim.md','Windows-audio-driver-help.md'),(f'docs/release-{version}.md','Release-notes.md')]:
 (out/dest).write_bytes((root/source).read_bytes())
assets=sorted(p for p in out.rglob('*') if p.is_file() and p.name!='SHA256SUMS' and (p.suffix in ('.deb','.rpm','.run') or p.name.endswith(('-setup.exe','-source.tar.gz')) or p.name.startswith('qtbase') and p.name.endswith('.tar.xz') or p.name in ('Release-notes.md','Linux-installation.md','Windows-audio-driver-help.md')))
(out/'SHA256SUMS').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in assets))
print(f'Prepared {name} {version} companions and checksums')
