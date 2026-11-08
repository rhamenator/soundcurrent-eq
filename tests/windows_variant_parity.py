#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("variant_parity", ROOT / "scripts/verify-windows-variant-parity.py")
module = importlib.util.module_from_spec(spec)
assert spec.loader
spec.loader.exec_module(module)

common = {"soundcurrent.exe": "1" * 64, "Qt6Core.dll": "2" * 64}
base = {"schema": 1, "product": "SoundCurrent", "version": "1.3.0", "sourceCommit": "a" * 40, "commonPayload": common}
cable = {**base, "audioRoute": "Cable", "installer": "cable.exe", "installerSha256": "3" * 64}
native = {**base, "audioRoute": "Native", "installer": "native.exe", "installerSha256": "4" * 64}
assert module.verify(cable, native)["commonPayloadFiles"] == 2

for field in ("version", "sourceCommit", "commonPayload"):
    changed = dict(native)
    changed[field] = "wrong" if field != "commonPayload" else {"soundcurrent.exe": "5" * 64}
    try:
        module.verify(cable, changed)
    except SystemExit:
        pass
    else:
        raise AssertionError(f"mismatched {field} was accepted")

colliding = dict(native); colliding["installer"] = cable["installer"]
try:
    module.verify(cable, colliding)
except SystemExit:
    pass
else:
    raise AssertionError("colliding variant names were accepted")

print("PASS: Windows cable/native version, source, payload and distinct-name parity")
