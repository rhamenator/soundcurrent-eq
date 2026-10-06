# Interim Windows route: VB-CABLE

The default Windows installer uses VB-Audio's signed primary cable until SoundCurrent driver signing is affordable. Native driver source and its separate installer remain preserved; see [native bookmark](windows-native-driver-bookmark.md).

## User experience

- **Audio driver setup** opens VB-Audio's signed setup if the primary cable is missing. Approve Windows UAC and click Install Driver. Restart Windows when requested.
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
