#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the easy installer against a local package in a disposable container."""
import hashlib
import json
import os
from pathlib import Path
import pty
import re
import select
import subprocess
import sys
import tempfile
import time

if os.geteuid() != 0 or not Path('/.dockerenv').exists():
    raise SystemExit('This fixture requires root inside a disposable Docker container')
root = Path(__file__).resolve().parents[1]
package = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
assert package.is_file() and package.suffix in ('.deb', '.rpm')
version = re.search(r'project\(soundcurrent-[a-z]+ VERSION ([0-9.]+)', (root/'CMakeLists.txt').read_text()).group(1)
code = (root/'scripts/install-linux.run').read_text()
# Apply the same version/hash substitutions used in companion assembly. No
# download occurs: --package-file supplies the package built by this job.
code = re.sub(r"^version='[^']+'", "version='" + version + "'", code, flags=re.M)
key = 'deb_hash' if package.suffix == '.deb' else 'fedora_hash' if '.fc44.' in package.name else 'rhel_hash'
digest = hashlib.sha256(package.read_bytes()).hexdigest()
code = re.sub(r"^" + key + r"='[^']+'", key + "='" + digest + "'", code, flags=re.M)
catalog = json.loads((root/'data/localization/linux-installer.json').read_text())['languages']['fr']
with tempfile.TemporaryDirectory() as directory:
    installer = Path(directory)/'installer.run'
    installer.write_text(code)
    master, slave = pty.openpty()
    env = dict(os.environ, DISPLAY='', WAYLAND_DISPLAY='', SC_INSTALLER_LANGUAGE='fr')
    child = subprocess.Popen(['bash', str(installer), '--package-file', str(package)],
                             stdin=slave, stdout=slave, stderr=slave, env=env)
    os.close(slave)
    transcript = bytearray()
    accepted = False
    try:
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            if select.select([master], [], [], 0.1)[0]:
                try:
                    chunk = os.read(master, 65536)
                except OSError:
                    break
                if not chunk:
                    break
                transcript.extend(chunk)
                if not accepted and catalog['continue'].encode() in transcript:
                    os.write(master, b'Y\n')
                    accepted = True
            elif child.poll() is not None:
                break
        result = child.wait(timeout=5)
        out.mkdir(parents=True, exist_ok=True)
        (out/'easy-installer-update.log').write_bytes(transcript)
        assert accepted, 'Localized terminal confirmation not observed'
        assert result == 0, 'Easy installer failed; inspect retained transcript'
        assert version in transcript.decode('utf-8', errors='replace')
        assert catalog['installed'].split('%1')[0].encode() in transcript
        (out/'easy-installer-update.json').write_text(json.dumps({
            'result':'passed','locale':'fr','packageSha256':digest,
            'confirmationAccepted':True,'exitCode':result,
            'scope':'Actual installer main, local package checksum verification and package-manager install/update inside disposable container; no download.'}, indent=2)+'\n')
    finally:
        if child.poll() is None:
            child.kill()
            child.wait()
        os.close(master)
