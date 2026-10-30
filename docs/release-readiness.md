# Release review and supported platforms

Status as of October 27, 2026: the owner’s personal review remains in progress.
Existing published installers remain available. Neither a historical 1.x version
label nor green CI is evidence of completed owner review or production acceptance.
This documentation update does not retag, downgrade, replace, or republish artifacts.

## Platform matrix

| Target | Available artifact | Qualification boundary |
| --- | --- | --- |
| Windows x64 | NSIS installer | Hosted clean install, launch/UI, in-place reinstall, uninstall and settings-retention fixtures; physical driver/audio review remains incomplete. |
| Ubuntu 24.04 / compatible Debian x64 | DEB and Linux installer script | Disposable-container install, launch/UI, update, uninstall and reinstall; dependency compatibility varies by distribution. |
| Fedora 44 x64 | RPM and Linux installer script | Disposable-container package lifecycle and installed UI qualification. |
| RHEL 10-compatible x64 | RPM and Linux installer script | AlmaLinux 10 disposable-container package lifecycle and installed UI qualification. |

Windows and Linux are the supported operating-system scope. macOS is excluded
from support claims, implementation commitments, and release gates. This does not
imply compatibility with every Linux distribution, architecture, or audio device.

## Existing evidence

The 1.1.1 release contains native package/installer artifacts, SHA256SUMS, source,
Qt source companions, and Windows audio setup/recovery instructions. Public
asset names, sizes, and GitHub SHA-256 digests were rechecked against the retained
publication receipt on October 27; all 11 assets matched. This is an artifact
integrity comparison, not a new install/uninstall or audio-device test.

Build and lifecycle records are retained in the repository’s GitHub Actions runs:

- Linux package qualification: https://github.com/rhamenator/soundcurrent-eq/actions/runs/39530224601
- Windows installer qualification: https://github.com/rhamenator/soundcurrent-eq/actions/runs/39530229756

These workflows qualify clean container/hosted-runner fixtures and intentional
settings retention. Reinstalling the same candidate is not proof of upgrade
compatibility from every previously published version. Container package removal
is not proof of complete host residue removal, and Windows hosted tests do not
prove physical VB-CABLE installation, routing, reboot recovery, or shared-driver
uninstall behavior. Checksums are not build-provenance attestations; no signed
build attestation is claimed by these workflows.

## Remaining review gates

Before claiming production readiness, retain exact-artifact evidence for supported
systems: clean install and launch; upgrade from previously distributed versions;
uninstall and reinstall; expected versus unexpected residue; deliberate retention
of user profiles/settings; Windows shared-driver ownership and reboot handling;
checksums and build/source correspondence; security and failure-path review; and
the owner’s personal acceptance. Record failures and unresolved gaps explicitly.

The SoundCurrent Windows application/installer remains unsigned. The interim
VB-CABLE route uses its vendor-signed driver; the preserved SoundCurrent native
driver is not a shipped signed replacement. Do not confuse those signing states.
Native-speaker translation verification and independent human review remain
unverified unless separately recorded. Do not use missing macOS support as a gate.

## Required PR checks

Required pull-request workflows run for documentation-only changes too. Do not
filter an entire required PR workflow by changed paths: GitHub may otherwise
leave its merge gate waiting for a check that never starts. Manually dispatched
branch checks are useful verification but do not necessarily satisfy the PR
merge-ref gate. Keep branch protections enabled.
