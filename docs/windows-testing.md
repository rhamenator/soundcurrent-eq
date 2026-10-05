# Windows preview verification

Version 0.7.0 builds the shared Qt interface on Windows. Speaker and
amplifier profile selection, adjustable frequency/Q controls, level meters,
microphone EQ and the calibration preview use the same UI as Linux. Windows
uses its own WASAPI backend and requires a separately installed second cable
for simultaneous microphone processing. A build or UI test does not establish
that room calibration or hotplug works with every physical device.

Version 0.7.0 was checked in an independent Windows 11 26H2 clone with the
signed VB-CABLE driver and a virtual High Definition Audio output. The native
Microsoft compiler build measured +6.05 dB live post gain, -11.95 dB EQ cut,
0.05 dB bypass difference, full-left balance, successful stop/restart, restoration
of all three playback roles, and a 1.007-second stereo calibration signal.
The installed UI check passed for tabs, controls, shortcuts, background
close/reopen, a single running instance and graceful quit. Real output hotplug,
a second microphone cable and room measurements still need physical-hardware
verification.

The bundled-driver installer has also been checked in that clone with the
driver present and absent: default selection, opting out, cancelling UAC,
approving the vendor install, retry shortcuts, and finishing with restart
deferred. No original Copperfin VM was modified for these installer checks.

## Automated checks

Build on Windows with `scripts/build-windows.ps1 -QtPrefix <Qt SDK path>`.
The script tests the shared DSP and the Qt UI, with bounded process waits.
The packaged application includes Qt and the Microsoft app-local runtime.
CTest also provides `dsp-response` and `shared-ui` checks.

The opt-in `build-windows-native/Release/soundcurrent-windows-audio-smoke.exe` integration
check lists endpoints by default. Run it with `--run` only in an isolated,
signed-in Windows test system with VB-CABLE installed and a physical output
available. Quit other copies of SoundCurrent EQ first. It plays a quiet
1 kHz tone at approximately -34 dBFS into CABLE Input and measures the
physical output through WASAPI loopback. It temporarily changes all three
Windows default playback roles to the cable and verifies their restoration
before testing the bridge. It checks Flat playback, a live +6 dB post-gain
change, a -12 dB EQ cut, bypass, full-left balance, stopping/restarting the
bridge, and the duration of a one-second stereo calibration signal. An
unsigned-in Windows VM returned silence from the cable; signing into its
desktop restored normal audio.

Run `tests/windows_ui_smoke.ps1` from the signed-in desktop of an isolated
Windows test system. Set the Windows default output to the physical speakers
first and quit the app. An optional `-InstallerPath` argument installs the
preview silently as the current user before the check. An optional
`-ResultPath` chooses where to save the result. The test verifies desktop
and Start menu shortcuts, runs the shared Qt control checks with isolated INI
settings, then verifies closing to the notification area, restoring the
existing instance and gracefully quitting through the app's activation channel.
The shared UI check also verifies that the Equalizer tab opens first with the
bands at the top, that device and sweep controls belong to Settings & calibration,
and that wheel movement scrolls the active page without changing a control.
It backs up the Windows registry settings before the desktop check and
restores them after successful exit. Use only disposable test settings; on failure it leaves
the app running so its state can be inspected.

`tests/windows_installer_smoke.ps1 -InstallerPath <setup.exe>` checks the
interactive installer in a signed-in independent clone. With the default
`-CableState Present`, it verifies detection and skipping of an existing
standard cable. After explicitly removing the driver and rebooting that
clone, use `-CableState Missing` to verify the default offer and opting out.
It checks visible vendor notices and the retry shortcut. By default, it opts
out without installing or removing any driver. On the independent clone,
`-CableState Missing -InstallDriver` exercises the real install step: manually
approve UAC, click Install Driver, and acknowledge the restart notices. Also
check cancelling elevation and restarting after a successful install.

## Check on real Windows hardware

1. Install the SoundCurrent EQ preview. When VB-CABLE is missing, keep the
   driver option checked, approve Windows' administrator prompt, and click
   Install Driver in VB-Audio's setup. Restart Windows when requested. On an
   existing installation, check that setup detects and skips the driver.
2. Start with your physical speakers as Windows' default output. Select your speakers or
   headphones in SoundCurrent EQ and start with Flat, 0 dB post gain, and
   centered balance. The app selects the cable while running. Play ordinary
   music at a comfortable volume.
3. Confirm that Flat sounds normal, that each named preset changes the
   sound, and that dragging post gain changes the level during playback.
   Check both ends of balance and restore it to center.
4. Toggle Equalizer on/off. Close the window, confirm playback continues,
   and reopen it from its icon or desktop shortcut. Lock the controls and
   confirm accidental wheel movement does not change a band.
5. Try a second physical output if available, including connecting and
   disconnecting it. Confirm the app's device list and Automatic behavior.
6. Use Quit. Confirm the prior Windows default output is restored and the
   app process exits. Also change Windows' default yourself before quitting
   and confirm that your subsequent choice is preserved.
7. With a separate cable installed, select a mono/stereo microphone and the
   microphone cable, then enable mic EQ. Check both routes together. Without
   a second cable the app should report that one is required.
8. Put the microphone at the listening position, stop music and use a quiet
   sweep. Reject measurements that are clipped or too close to background
   noise. Review the proposed changes before applying them.

The app installer is currently unsigned. The audio driver is separately
signed by its vendor. The installer and checksum should be obtained from
the project's GitHub release page.
