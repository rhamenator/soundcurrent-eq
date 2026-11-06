# SoundCurrent EQ 1.3.0

- Add a dedicated analyzer tab with rolling spectrogram, connected spectrum, waveform, tuner, vectorscope, averaged-spectrum and cumulative microphone-reference views. Capture and analysis stop when the tab is inactive; settings/profile dialogs pause capture.
- Carry the C++ spectrogram app's analysis/display settings and legacy preset import. The rolling image follows display resolution, preserves narrow peaks and scales smoothly on high-DPI displays. Readable Hz/kHz labels follow linear, logarithmic or octave frequency axes; the time axis marks the newest audio at 0 s.
- Add estimated equipment profiles: subtract measured component responses from a measured whole-chain response over their common range, retain source identities/hashes and conditions, and review/edit the correction before saving. Estimated labels survive renaming, editing and export/import. Source responses and editable corrective EQ remain separate. Saving does not automatically apply a correction.
- Add manufacturer/type/search filters and a checked-component summary to the estimator. Microphone-reference estimates also open in the existing profile editor before saving.
- Check for updates automatically at startup and show an Update shortcut left of Quit when a newer release or downloaded installer is available. Existing configuration update buttons remain. The shortcut opens official downloads rather than executing an unverified local installer.
- Make the listening response graph taller, automatically scale its vertical range, and allow hiding it. Split equipment, microphone and speaker/room setup into focused dialogs with pinned Close controls.

## Upgrade

Use **Quit app**, then install over the existing app. Closing the window can leave background processing active. Settings and custom profiles are retained. Linux downloads include the easy `.run` installer, Ubuntu/Debian DEB, Fedora 44 RPM and RHEL 10-compatible RPM; Windows includes an x64 installer. Updating a source checkout alone does not upgrade an installed Windows application.

## Measurement and platform limits

An estimated response can still contain unknown room, microphone, clothing, placement and interface effects. It is not a certified isolated device measurement. Correction-only presets cannot serve as measured component responses. The estimator requires at least six points per input and two octaves of common measured coverage, does not extrapolate, and retains conservative EQ limits. Estimated profiles use JSON schema 4; older app versions reject that schema rather than silently remove the estimate label.

A sharper spectrogram does not create additional FFT resolution. The FFT-size setting controls measured frequency separation. Analysis monitors at most two channels; Studio's separate multichannel processing remains available. The Linux development host has stereo hardware; physical Windows capture, arbitrary-channel equipment and acoustic calibration require hardware verification.

All 34 interface catalogs are populated; linguistic accuracy remains unverified. The reused analyzer core is GPL-3.0 and credited in the notices and packaged license. No proprietary instruments, algorithms or equipment assets are introduced.

Windows still uses the interim signed VB-CABLE driver; SoundCurrent's future native-driver source remains preserved. If media keys move the Windows fader without changing loudness, enable **Windows Volume Control** in the vendor's control panel (see the accompanying audio-driver guide). This release does not claim that the older Windows control report has been reproduced or fixed on the user's physical machine. For audible Studio changes, leave **Offline editing** unchecked and launch normally without `--preview`.
