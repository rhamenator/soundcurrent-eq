# Free edition: Linux and Windows feature parity

The default `main` branch builds SoundCurrent EQ for both platforms. Both use
`src/main.cpp` for the desktop controls and `src/dsp.cpp` for the C++ filters.
Windows no longer depends on a separate reduced interface. The older
`windows-port` branch and 0.7.0 preview contain the history of this integration.

| Free feature | Linux | Windows |
| --- | --- | --- |
| Flat default and 34 listening presets, including Loudness/Night Listening | Shared UI | Shared UI |
| 5–31 bands with editable frequency, gain and Q; saved presets | Shared UI | Shared UI |
| Immediate post-gain slider and stereo balance | PipeWire parameters | Shared C++ DSP through WASAPI |
| Colorized frequency/overall meters, optional peak markers, 1–500 ms refresh | PipeWire monitor tap | WASAPI processing tap |
| Output dropdown and automatic connected-device selection | PipeWire device list | Windows endpoint list |
| Speaker model profiles and measured amplifier-profile imports | Shared parser/filters | Shared parser/filters |
| Microphone natural-voice EQ and input gain | Separate virtual microphone | Separate virtual microphone cable |
| Quiet sweep/tones, room-noise/clipping checks and preview before applying | Shared analysis, PipeWire recording/playback | Shared analysis, WASAPI recording/playback |
| Lock, Undo and protection from accidental wheel adjustments | Shared UI | Shared UI |
| Equalizer first tab; settings/calibration second tab; fit to working area | Shared UI | Shared UI |
| Background close/reopen, single instance and explicit Quit | Tray and activation socket | Notification area and activation socket |
| Installer and application shortcuts | DEB/RPM/app menu | User installer, desktop and Start menu |

## Platform requirements

Linux uses PipeWire and its PulseAudio compatibility tools. Windows uses the
selected endpoint's shared-mode format/sample rate and the signed VB-CABLE
virtual route. The Windows installer offers the standard cable's verified vendor
setup when it is missing. Simultaneous Windows speaker and microphone EQ requires
a separately installed second cable; the same standard cable cannot carry both
routes. Playback is stereo in this free edition.

## Verification

Both platform builds run the shared DSP and Qt control checks. Windows installer,
background lifecycle and live stereo route results are recorded in
[Windows testing](windows-testing.md). Linux can run the UI checks offscreen
without switching the current speaker route.

A build or VM test does not verify every physical driver, hotplug sequence,
second microphone cable or room measurement. These remain hardware test items.
The private Studio edition's channel router, delay/reverb, 256-channel renderer
and engine SDK are developed separately.
