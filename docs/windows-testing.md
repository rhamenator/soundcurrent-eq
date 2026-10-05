# Windows preview verification

The Windows preview has been checked in an independent Windows 11 26H2
clone with the signed VB-CABLE driver and a virtual High Definition Audio
output. Real speaker hardware, device hotplug, microphone EQ, and room
calibration require further work. The additional Linux features listed in
the README are not yet available on Windows.

## Automated checks

Build with `./scripts/build-windows.sh`. CTest's `dsp-response` check verifies
the DSP without needing an audio device. The cross-built test executables
include the MinGW runtime and can run directly on Windows.

The opt-in `build-windows/soundcurrent-windows-audio-smoke.exe` integration
check lists endpoints by default. Run it with `--run` only in an isolated,
signed-in Windows test system with VB-CABLE installed and a physical output
available. Quit other copies of SoundCurrent EQ first. It plays a quiet
1 kHz tone at approximately -34 dBFS into CABLE Input and measures the
physical output through WASAPI loopback. It does not change Windows' default
output. It checks Flat playback, a live +6 dB post-gain change, a -12 dB EQ
cut, bypass, full-left balance, and stopping/restarting the bridge. An
unsigned-in Windows VM returned silence from the cable; signing into its
desktop restored normal audio.

Run `tests/windows_ui_smoke.ps1` from the signed-in desktop of an isolated
Windows test system. Set the Windows default output to the physical speakers
first and quit the app. An optional `-InstallerPath` argument installs the
preview silently as the current user before the check. An optional
`-ResultPath` chooses where to save the result. The test verifies desktop
and Start menu shortcuts, all 34 presets, gain direction and labels,
lock/undo, bypass labels, wheel protection, the preset popup, closing to the
notification area, restoring the existing instance, and quitting. It
temporarily resets the app's current settings and restores them after a
successful check. Use only disposable test settings; on failure it leaves
the app running so its state can be inspected.

## Check on real Windows hardware

1. Install the signed VB-CABLE driver from its official site and restart
   Windows when requested. Install the SoundCurrent EQ preview.
2. Set CABLE Input as Windows' default output. Select your speakers or
   headphones in SoundCurrent EQ and start with Flat, 0 dB post gain, and
   centered balance. Play ordinary music at a comfortable volume.
3. Confirm that Flat sounds normal, that each named preset changes the
   sound, and that dragging post gain changes the level during playback.
   Check both ends of balance and restore it to center.
4. Toggle Equalizer on/off. Close the window, confirm playback continues,
   and reopen it from its icon or desktop shortcut. Lock the controls and
   confirm accidental wheel movement does not change a band.
5. Try a second physical output if available, including connecting and
   disconnecting it. Confirm the app's device list and Automatic behavior.
6. Restore your physical speakers as Windows' default output, then use
   Quit. Confirm normal playback continues and the app process exits.

The app installer is currently unsigned. The audio driver is separately
signed by its vendor. The installer and checksum should be obtained from
the project's GitHub release page.
