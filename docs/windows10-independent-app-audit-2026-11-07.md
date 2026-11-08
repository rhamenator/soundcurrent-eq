# Independent Windows 10 app/UI audit — 7 November 2026

**Ordinary app installation and UI passed; native-driver and real-audio support on Windows 10 remain unverified.** This record does not establish production signing, physical microphone behavior, or hardware latency.

## Frozen environment and inputs

A new, independent disposable QEMU VM ran Windows 10 Pro 22H2 x64, build **19045.2965**, with 4 GiB RAM and two virtual CPUs. Secure Boot remained enabled (registry value 1). No product key, activation, Microsoft-account sign-in, test certificate, test-signing mode or audio-driver installation was used. The virtual sound device had no host audio backend. Network was disconnected during setup/testing. Only one VM ran; disk I/O was capped at 16 MiB/s.

Official [Microsoft Windows 10 media](https://www.microsoft.com/en-us/software-download/windows10ISO): `Win10_22H2_English_x64v1.iso`, SHA-256 `a6f470ca6d331eb353b815c043e327a347f594f37ff525f17764738fe812852e`. This ISO is not redistributed in the repository.

| App | Tested feature code | CI artifact run |
| --- | --- | --- |
| EQ | `874f5ab` | [42427390911](https://github.com/rhamenator/soundcurrent-eq/actions/runs/42427390911) |
| Studio | `9037029` | [42427391968](https://github.com/rhamenator/soundcurrent-studio/actions/runs/42427391968) |

The branch installers still carry **1.3.0** filenames. They are identified by source/run/hash, not confused with the existing public 1.3.0 release. Exact app and installer SHA-256 values are in `eq-provenance.json` and `studio-provenance.json` in the evidence directory. Each installed app matched its CI payload hash. Windows builds pin Qt 6.12.0; [Qt documents Windows 10 support in 6.12](https://doc.qt.io/qt-6.12/windows.html), but that documentation alone is not the installation proof.

## Observed results

Both apps passed all ten checks: install, main-window UI self-test, microphone policy/persistence, Windows capture-backend denial before endpoint activation, responsive-dialog checks, Arabic localization UI, uninstall, reinstall, in-place update, and final uninstall. Every process returned 0. Final checks found no app executable or uninstall registration. Supplemental signing-input validation passed on **Windows PowerShell 5.1.19041.2673**, without a key, certificate, WDK or driver action.

Tests used native Windows Qt rendering and 150% font scaling. UI self-tests operate with isolated settings and no live capture/processing. The apps were tested sequentially. Selected synthetic UI screenshots were exported read-only through the owned serial console and inspected, including the pinned Quit and Studio output controls. The raw screenshots confirm Quit is present; no clipping defect was established from these images.

Evidence: [`tests/results/windows10/2026-11-07-independent-874f5ab-9037029/`](../tests/results/windows10/2026-11-07-independent-874f5ab-9037029/). Text is normalized to UTF-8/LF where needed; original archive hashes and individual file hashes are recorded. The initial attempt is retained separately: Windows PowerShell's Start-Process wrapper supplied a null exit code, so that attempt was not counted. The working runner uses System.Diagnostics.Process, retains the process handle and drains redirected output asynchronously.

## Reproduce safely

Use a fresh, independent disposable VM, never an original/template or an installation with existing SoundCurrent apps. The runner requires explicit `-DisposableVM`, QEMU manufacturer metadata, the expected Windows 10 build, no pre-existing installs, and a new output directory. It does not configure an audio driver or Windows trust.

1. Download the two specified `windows-build` artifacts. Expand them as `MEDIA/EQ` and `MEDIA/STUDIO`, retaining their `dist` and `build-windows-native` subdirectories.
2. Copy the evidence directory's `signing-extras` contents into MEDIA. These are frozen, no-signing test helpers from Studio's native-driver audit (`2472c7b`), not a backport of the shipping EQ installer/manager.
3. Put the runner at MEDIA root as `AUDIT10.PS1` and expose MEDIA read-only as a guest drive (D: in this run). For a new build pass its recorded code revisions explicitly; do not relabel old binaries as new ones.
4. Start `tests/windows10_serial_results.py --vm YOUR_DISPOSABLE_VM --output HOST_RESULTS` on the owning Linux host. It needs python3-libvirt; the channel is the VM serial console, with no network listener. Archives have compressed/uncompressed size, member/path/type and PNG-dimension limits.
5. In the VM's normal, unelevated desktop session, run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File D:\AUDIT10.PS1 `
  -DisposableVM -MediaRoot D:\ -OutputDirectory "$env:USERPROFILE\SoundCurrentAudit10" `
  -EqRevision 874f5ab -StudioRevision 9037029
```

ExecutionPolicy is process-scoped; no persistent policy change is made. Keep the VM offline. The optional `windows10_collect_ui.ps1` exports only the named synthetic screenshot files from a successful result directory. Shut the VM down after testing. No credentials belong in either repository or result bundle.

## Limits and follow-up

* This was an **app-only** ordinary-installer test. VB-CABLE driver installation/uninstallation/audio, the native SYS package on Windows 10, physical microphone indicators/hotplug, channel hardware, real latency and mixed-DPI monitor movement are not qualified by it.
* The native INF/manager currently require Windows 11 build 22621+. Lowering the floor requires a rebuilt, matching catalog/signature and separate runtime test. Windows 10 x64 is not a general exemption from kernel signing.
* Later Linux helper-lifetime hardening and additional test assertions have their own CI coverage. These recorded binaries are not represented as byte-identical later-head installers. Requalify a final release candidate after integrating the native/volume-control branches.
* Font-scale and synthetic tests do not establish linguistic translation accuracy. Catalogs remain labeled unverified pending native-speaker review.
