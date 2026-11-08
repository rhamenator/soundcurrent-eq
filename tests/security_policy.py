#!/usr/bin/env python3
"""Fail-closed repository and Windows-driver security contracts."""
from __future__ import annotations

import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    raise SystemExit(f"SECURITY POLICY FAILED: {message}")


def text(relative: str) -> str:
    path = ROOT / relative
    if not path.is_file():
        fail(f"required file is missing: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


cmake = text("CMakeLists.txt")
match = re.search(r"project\(soundcurrent-(eq|studio) VERSION ([0-9]+\.[0-9]+\.[0-9]+)", cmake)
if not match:
    fail("cannot determine product and version from CMakeLists.txt")
product, project_version = match.groups()
installer_stem = "soundcurrent-eq" if product == "eq" else "soundcurrent-studio"

installers: dict[str, str] = {}
for variant in ("cable", "native"):
    suffix = "" if variant == "cable" else "-native"
    relative = f"packaging/windows/{installer_stem}{suffix}.nsi"
    source = text(relative)
    version = re.search(r'!define APP_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"', source)
    if not version or version.group(1) != project_version:
        fail(f"{variant} installer version must equal project version {project_version}")
    for contract in ("RequestExecutionLevel user", "StrCpy $InstallDriver 0"):
        if contract not in source:
            fail(f"{variant} installer lost contract: {contract}")
    installers[variant] = source

build = text("scripts/build-windows.ps1")
for contract in (
    "[ValidateSet('Cable','Native')]",
    "soundcurrent-driver-manager.exe",
    "Get-AuthenticodeSignature",
    "& $SignedDriverManager --verify $SignedDriverPackage",
    "VBCABLE_Driver_Pack45.zip",
    "b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb",
):
    if contract not in build:
        fail(f"Windows build lost variant/signing contract: {contract}")
for contract in ("windows-x64$installerSuffix-setup.exe", "variantManifest", "commonPayload", "sourceCommit"):
    if contract not in build:
        fail(f"Windows build lost same-source variant manifest contract: {contract}")
if "installerSuffix = if ($AudioRoute -eq 'Native') { '-native' }" not in build:
    fail("native and cable installers must have distinct filenames")
text("scripts/verify-windows-variant-parity.py")
if "dist/*-manifest.json" not in text(".github/workflows/windows-check.yml"):
    fail("Windows variant manifests must be retained as CI artifacts")
if "*-manifest.json" not in text(".github/workflows/release.yml"):
    fail("Windows variant manifests must be published with releases")
if "-manifest.json" not in text("scripts/assemble-release.py"):
    fail("Windows variant manifests must be covered by release checksums")

wrapper = text("packaging/windows/native-audio-setup.ps1")
for contract in ("& $helper --verify $package", "Get-AuthenticodeSignature", ".Status -ne 'Valid'", "-Verb RunAs"):
    if contract not in wrapper:
        fail(f"native setup wrapper lost contract: {contract}")

manager = text("native/windows/driver-manager/main.cpp")
for contract in (
    "WTD_REVOKE_WHOLECHAIN",
    "SetupVerifyInfFileW",
    "verifyCatalogMember",
    "FILE_ATTRIBUTE_REPARSE_POINT",
    "WindowsAudioSessionGate",
    "UpdateDriverForPlugAndPlayDevicesW",
    'hardware[]=L"Root\\\\SOUNDCURRENTVAD"',
):
    if contract not in manager:
        fail(f"privileged manager lost contract: {contract}")

privileged_sources = "\n".join((wrapper, manager))
for forbidden in (
    r"\bbcdedit\b",
    r"\btestsigning\b",
    r"Set-ExecutionPolicy",
    r"Add-MpPreference",
    r"INSTALLFLAG_FORCE",
):
    if re.search(forbidden, privileged_sources, re.IGNORECASE):
        fail(f"privileged runtime contains forbidden security-policy mutation: {forbidden}")

tracked = subprocess.run(
    ["git", "-C", str(ROOT), "ls-files"], check=True, capture_output=True, text=True
).stdout.splitlines()
private_suffixes = (".pfx", ".p12", ".pvk", ".key", ".jks", ".keystore")
for relative in tracked:
    lower = relative.lower()
    if lower.endswith(private_suffixes) or pathlib.PurePosixPath(lower).name in {"id_rsa", "id_ed25519"}:
        fail(f"private key container is tracked: {relative}")

pin = re.compile(r"^[0-9a-f]{40}$")
for workflow in sorted((ROOT / ".github/workflows").glob("*.y*ml")):
    for line_number, line in enumerate(workflow.read_text(encoding="utf-8").splitlines(), 1):
        use = re.search(r"\buses:\s*([^\s#]+)", line)
        if not use or use.group(1).startswith("./"):
            continue
        ref = use.group(1).rsplit("@", 1)
        if len(ref) != 2 or not pin.fullmatch(ref[1]):
            fail(f"external action is not pinned to a commit: {workflow.name}:{line_number}")

print(f"PASS: {product} {project_version} variant parity, privileged-driver trust contracts, key inventory, and action pins")
