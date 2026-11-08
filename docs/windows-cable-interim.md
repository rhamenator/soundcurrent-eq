# Interim Windows route: VB-CABLE

The default Windows installer uses VB-Audio's signed primary cable until SoundCurrent driver signing is affordable. Native driver source and its separate installer remain preserved; see [native bookmark](windows-native-driver-bookmark.md).

## User experience

- **Audio driver setup** opens VB-Audio's signed setup if the primary cable is missing. Approve Windows UAC and click Install Driver. Restart Windows before using either equalizer or opening VB-CABLE settings. Setup records a pending restart for both apps and displays an explicit reminder.
- **Audio driver setup from the app** pauses processing and keeps the window open. Helper failures and restart requirements appear in a dialog; closing the UI is unnecessary.
- **VB-CABLE settings** opens the official control panel from either application's device settings or its Start menu folder. Adjust internal sample rate and latency there. These settings are shared by every cable client; changes can interrupt playback. The app does not rewrite undocumented driver settings.
- Choose physical speakers/headphones inside SoundCurrent. Output selection, automatic switching, gain, On/Off, crash recovery, and app exclusion use the existing WASAPI implementation.
- Simultaneous speaker and microphone processing requires a separately installed second cable. A/B and C/D packages are not bundled.
- Interactive uninstall offers primary cable removal through the official vendor setup when the other SoundCurrent app is absent for this user. Confirm that no other users or programs need it, then click Remove Driver. A cancelled/incomplete removal keeps SoundCurrent available for retry. Reboot if requested, then retry if Windows still lists the cable.
- Silent app uninstall leaves shared drivers installed. Existing unrelated VB-Audio products and hardware drivers are preserved.

## Building

Run `scripts/build-windows.ps1 -QtPrefix C:\Qt\6.12.0\msvc2022_64`. The builder downloads the unchanged original primary cable archive from VB-Audio if absent and verifies SHA-256 `b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb`. Setup verifies the archive and Windows Authenticode before opening vendor programs. Only the signed vendor setup requests elevation. The ordinary SoundCurrent app/installer remains unsigned until app signing is arranged; Windows may show publisher warnings.

Native builds require explicit `-AudioRoute Native -SignedDriverPackage ... -SignedDriverManager ...`; the signing requirements are unchanged.

## Licensing

VB-CABLE remains proprietary donationware, separate from GPL SoundCurrent. Its notice, origin, and donation link ship with the package. [VB-Audio's distribution rules](https://vb-audio.com/Services/licensing.htm) allow the primary package with an application when the donation model remains visible/applicable; professional deployments have additional licensing requirements. [Official manual](https://vb-audio.com/Cable/VBCABLE_ReferenceManual.pdf) documents the control panel, elevated install/removal, and reboot requirements. No purchase or enrollment was performed.

## Incomplete installation after restart

A retained VB-CABLE PnP driver record does not prove that playback and recording work. Setup checks for active CABLE Input and CABLE Output endpoints. A driver record with missing endpoints is reported as incomplete, not already installed.

Check that both endpoints are enabled in Windows Sound settings. If they remain missing after restarting, Audio driver setup opens the signed official installer with repair instructions: Remove Driver, restart Windows, run Audio driver setup and Install Driver, then restart again. Removing the shared cable affects other applications that use it. SoundCurrent does not remove it silently. The settings button reports the missing endpoints and keeps the app open.

## Windows volume and live controls

Windows setup reconciles the standard cable's optional Windows Volume Control
setting on fresh install, repair, and every app update, including app-only updates.
It saves DWORD `VBAudioCableWDM_VolCtl=1` in the explicit 64-bit view of
`HKLM\SOFTWARE\VB-Audio\Cable` and
`HKLM\SYSTEM\CurrentControlSet\Services\VB-Cable`, and reads both back.
The second location matches the vendor control panel and is the driver's
startup fallback; the Software value alone failed a real Windows reboot test. This integration is based on
the pinned Pack45 driver and the Windows-host diagnostic evidence recorded in
[the handoff](windows-volume-control-installer-handoff.md); the registry value is
not a vendor API promise for future driver packages. The fixed-purpose helper
checks the primary cable hardware ID and cannot accept arbitrary registry keys,
values or commands. Other cable products and settings are preserved.

Interactive setup requests administrator approval only when a write is needed.
Already-enabled settings need no write or elevation. Silent setup never shows
UAC or the vendor installer: elevated setup writes directly; unelevated setup
leaves the app installed and records deferred configuration. Setup saves the
result under `HKCU\Software\SoundCurrent\VBCable\VolumeConfigurationResult`:
`0` saved/already enabled, `3010` restart required, `740` administrator approval
required, `1223` approval cancelled, `31` configuration/readback failed. `10`
means the helper no longer sees the primary cable. An app-only install with no
cable performs no configuration and reports `0`.

For deferred or failed configuration, open **Audio driver setup** interactively,
approve elevation, and restart Windows. Do not reinstall the cable just to
configure volume. A volatile machine-wide pending marker shared by both apps
keeps the restart requirement until Windows reboots. The helper does not restart
devices, interrupt active streams, or claim that a saved setting has changed the
running driver. Uninstalling either app leaves the shared setting enabled.

The vendor panel's **Options → Enable Windows Volume Control** remains a manual
repair route; run it with administrator approval to persist the change. See the
[official manual, page 13](https://vb-audio.com/Cable/VBCABLE_ReferenceManual.pdf#page=13).
No second software gain stage is added.

Run the Windows installer to upgrade an installed app. Updating a Git checkout
only updates source files. Quit the old app first and launch the installed app
normally afterward: `--preview` intentionally disables live audio processing.
In Studio, uncheck **Offline editing** (or use **Apply settings**) when you want
changes to affect playback. A setup opened for review remains offline until
explicitly applied; unsupported live channel layouts also require a compatible
output device. Ordinary playback should be enabled and Studio bypass should be
off when testing audible effects.
