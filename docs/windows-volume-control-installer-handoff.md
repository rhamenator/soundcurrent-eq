# Handoff: persist VB-CABLE Windows Volume Control during setup

Status: implementation added on this PR branch. Original Windows-host evidence
below is preserved. Native audible and post-reboot acceptance remain pending;
controlled fixtures do not certify those behaviors.

Base reviewed: `d83759045621c0dc795e97b384d1d820c8ca66a7` (`origin/main`).
Apply the same behavior to `rhamenator/soundcurrent-studio`; keep both PRs aligned.

## Problem and observed fix

With SoundCurrent processing enabled, Windows uses CABLE Input as its playback
endpoint. VB-CABLE 3.3.1.7 can report changing endpoint volume/mute without
applying it to the signal when its optional Windows Volume Control is disabled.
Both the system controls and SoundCurrent's system-volume slider then appear
ineffectual. This concerns volume and mute, not play/pause or track-skip keys.

The installed vendor control panel reported `Volume Control: Disabled`.
Enabling its `Options > Enable Windows Volume Control` immediately restored
volume and mute. Unelevated changes affected the running driver but produced a
warning that administrator rights were necessary for persistence.

The saved configuration was missing the following value. An elevated write,
flush, and independent readback verified this machine-wide setting:

```text
Registry view: 64-bit
Key:   HKEY_LOCAL_MACHINE\SOFTWARE\VB-Audio\Cable
Name:  VBAudioCableWDM_VolCtl
Type:  REG_DWORD
Value: 1
```

The value name also occurs in the bundled vendor control-panel executable.
This is evidence for the pinned driver version, not a supported API guarantee
for all future versions. Verify its behavior against the shipped vendor package.
The first subsequent reboot retained this Software value but lost live volume
control. See the post-reboot correction below: saving this location alone was
insufficient. Both vendor persistence locations must be saved and verified,
and a running driver also needs its live configuration applied.

Vendor reference: <https://vb-audio.com/Cable/VBCABLE_ReferenceManual.pdf>,
Windows Volume Control section, page 13.

## Implementation scope

- Enable and persist Windows Volume Control for the standard VB-CABLE used by
  SoundCurrent, during fresh installation, repair, and application updates.
- Cover reuse of an already installed cable; do not require reinstalling it.
- Make configuration idempotent: absent, wrong-type, or disabled values become
  DWORD 1; an existing DWORD 1 requires no write or elevation.
- Use the explicit 64-bit registry view from both 32-bit NSIS and PowerShell.
  Restrict elevated writes to the validated standard-cable configuration.
  Keep authorization/path validation in any privileged helper; do not expose
  arbitrary registry-write or executable-launch capabilities.
- Preserve all unrelated vendor settings, endpoint levels/mute, audio routes,
  presets, and device configuration. Do not touch optional cable A/B products,
  Realtek drivers, security settings, or driver signing policy.
- Verify the saved value after writing. A cancelled elevation or failed write
  must produce a distinct, actionable configuration result, not silent success
  or an assertion that the installed driver itself failed.
- Interactive per-user setup may request elevation for machine-wide cable
  configuration. An elevated unattended install/update should configure it
  without dialogs. Unelevated unattended setup must not hang, spawn hidden UAC,
  or claim success for configuration it could not save; provide a documented
  deferred repair/status path while preserving the application's install.
- If the running driver needs restarting, report that requirement using the
  existing reboot/readiness contract. Do not force-reboot Windows, disrupt
  active streams unexpectedly, or equate a successful registry write with a
  verified live change. Do not introduce an additional software gain stage.
- Treat this as shared machine-wide cable configuration. Installing/updating
  either SoundCurrent app should reconcile it; uninstalling one must not disable
  it for the other app or other users.

## Entry points to inspect

- `packaging/windows/cable-setup.ps1`: `Present`, `Ready`, `NeedsReboot`, the
  `-Silent` early return, and the already-present/ready `-Install` early return.
- `packaging/windows/*.nsi`: per-user installer, driver checkbox, fresh-install
  and update flow, silent mode, and driver setup invocation. Do not wire the fix
  exclusively behind first-time driver installation or the driver checkbox.
- `src/windows_cable_setup_guard.cpp`: current readiness/elevation boundary.
- `tests/windows_cable_installer_smoke.ps1`, `tests/windows_incomplete_cable_smoke.ps1`,
  `tests/windows_audio_setup_smoke.ps1`, and Windows CI/release workflows.
- `docs/windows-cable-interim.md`: currently instructs users to enable the option
  manually. Update it to describe automatic configuration and genuine failures.
- Preserve localization conventions for any new installer messages.

## Acceptance evidence

1. Unit/fixture coverage for absent, zero, one, wrong-type, access-denied, and
   elevation-cancelled settings; include registry-view and readback failures.
2. Installer tests for fresh cable install and an existing cable with volume
   control disabled; test an app-only update and repeat setup without reinstall.
3. Cover elevated silent setup and unelevated silent setup with observable status;
   assert neither unattended path blocks waiting for an interactive dialog.
4. On native Windows, confirm the vendor panel reports enabled and Windows
   volume/mute affect audio while each installed app processes it. Record the
   difference between live enablement and persisted configuration.
5. Restart Windows and repeat volume/mute checks. Verify DWORD 1 remains present
   and the other installed SoundCurrent app continues to work. Include relevant
   reboot-required, incomplete-driver, coexistence, and uninstall regressions.

Do implementation and heavy builds on the development machine/CI. This Windows
machine was used for source inspection and the targeted configuration repair.

## Implementation evidence and outstanding acceptance

- Fixed-purpose native helper uses explicit 64-bit registry access, fixed DWORD
  and primary hardware identity; no caller-controlled write paths or values.
- Both NSIS installers configure existing cables on app-only updates and keep
  observable status for silent updates. Silent operations never request UAC.
- Native API fixture tests missing/zero/one/wrong-type settings, access denial,
  readback/flush failure, cancelled elevation, no silent UAC, idempotence, and
  the shared pending/reboot boundary without touching actual Windows settings.
- PowerShell tests extract and run the actual configuration function with an
  inert helper, checking exit codes remain numeric and quiet notices do not
  pollute them. New messages follow the existing 34-catalog path; translations
  remain linguistically unverified.
- Still required before release: fresh-driver and existing-driver installer
  exercises on native Windows, an app-only update with disabled volume control,
  audible volume/mute verification for both apps, an actual reboot and repeat
  verification, and coexistence/uninstall checks. CI without an installed cable
  cannot certify these. No physical-host result is claimed here.


## Post-reboot correction (2026-11-08)

The first real reboot retained the Software DWORD but reset the live volume
flag: 50% Windows volume measured 0.00 dB attenuation instead of -10.51 dB.
The signed Pack45 control panel writes the same DWORD to both the Software
location above and `HKLM\SYSTEM\CurrentControlSet\Services\VB-Cable`.
Static inspection of the installed signed 3.3.1.7 driver confirms that it
queries the Software location and then this SYSTEM fallback. The helper now
saves and verifies both fixed locations, migrating existing Software-only
configurations with explicit administrator approval, then applies the live
setting. Readiness requires both values and the live flag. This corrects the
previous incomplete persistence implementation; another full Windows reboot
is still required to verify the revised dual-location save on this PC.
