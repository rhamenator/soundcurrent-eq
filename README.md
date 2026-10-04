# SoundCurrent EQ

A simple, native C++ desktop equalizer for Ubuntu's PipeWire audio stack. It
starts with 15 frequency bands and lets you choose any count from 5 to 31.
Each band has adjustable gain, center frequency, and width (Q). It also has a
response curve, 33 listening presets, saved custom presets, and an output dropdown
with an **Automatic** option that follows connected devices.

![SoundCurrent EQ desktop window](docs/screenshot.png)

SoundCurrent EQ turns on when you open it and creates a PipeWire virtual sink. It routes that
sink to the selected physical output. Closing the window leaves it running in
the background; quitting from the tray icon restores the normal output. The
actual audio processing is performed by PipeWire's built-in filters.

## Install on Ubuntu

1. Download the `.deb` from the [latest release](https://github.com/rhamenator/soundcurrent-eq/releases/latest).
2. Open it in Ubuntu's package installer, or run:

   ```bash
   sudo apt install ./soundcurrent-eq_*.deb
   ```

3. Launch **SoundCurrent EQ** from the app menu. The equalizer starts on using
   the current output device. Choose **Automatic** or a specific output device.

The package declares its dependencies so `apt` installs the required Qt and
PipeWire tools. Releases include a `.sha256` checksum file. The app currently
targets 64-bit Ubuntu 24.04 LTS and newer with PipeWire audio.

If you download the checksum file alongside the package, verify it with
`sha256sum -c soundcurrent-eq_*.deb.sha256` before installing.

For a user-only install when the runtime dependencies are already present:

```bash
./scripts/install-user.sh ./soundcurrent-eq_*.deb
```

This extracts the package under `~/.local/share/soundcurrent-eq` and creates a
launcher in `~/.local/share/applications`. It does not use administrator access
or install missing dependencies.

## Use

- **Automatic** starts with the current default output. When a new output is
  connected, it switches to that device. If it disappears, it falls back to an
  available output. Choose a named device to keep the EQ on that device.
- Set **Bands** anywhere from 5 to 31. The current EQ shape is interpolated
  when the count changes, so your tuning is retained. The default is 15 bands.
- Move a slider or drag a point on the curve to adjust gain. Select a band and
  edit its center frequency, gain, or Q in the fields above the curve. Keyboard
  navigation works on the sliders and fields. Changes apply live without
  restarting the filter.
- Boosts automatically lower the preamp using the calculated combined response
  to leave headroom.
- Pick a built-in preset or save your own. Custom presets live in your user
  configuration directory. Saved nine-band presets from earlier releases can
  still be loaded.
- Built-in presets are grouped by everyday listening, bass and speech,
  entertainment, and music. They include Deep Bass, Podcast, TV Dialogue,
  FPS Footsteps, Rock, Jazz, Electronic, Hip-Hop, and more.
- Closing the window keeps the equalizer running. Use its indicator icon to
  reopen it, turn processing on or off, or quit. Launching the app again also
  reopens the existing window. If the desktop has no tray, closing the window
  exits and restores the normal output.
- Use **Equalizer on** to compare the processed sound with the normal output.

**Hardware note:** An equalizer changes the audio signal. It will not repair a
physical output that pops when its amplifier powers up. Select a different
output in the dropdown if one device has that behavior.

## Build from source

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev pipewire pipewire-bin pipewire-pulse wireplumber pulseaudio-utils
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/soundcurrent-eq
```

Build an installable package:

```bash
./scripts/build-deb.sh
```

The package and checksum appear in `dist/`. The GitHub release workflow builds
on Ubuntu 24.04 and attaches these files to each `v*` tag.

## Verify the interface and audio routing

```bash
./build/soundcurrent-eq --self-test
QT_QPA_PLATFORM=offscreen ./build/soundcurrent-eq --ui-self-test
python3 tests/audio_response.py ./build/soundcurrent-eq
```

The first command briefly creates the EQ sink, changes its band count and
controls, and checks that the original default output is restored. It uses the
currently selected physical output and does not play a test sound. The second
checks the 31-band interface, preset library, and selected-band controls without using audio.
The third command sends a 1 kHz tone through a temporary silent sink and
verifies that a 12 dB band cut changes the measured output by about 12 dB.
It requires `paplay` and `parec`, and leaves the normal default output alone.

## Privacy and safety

The app has no account, network service, or telemetry. It runs without root and
keeps temporary audio configuration in a private directory. The `.deb` only
installs the binary, launcher, and icon. See [SECURITY.md](SECURITY.md) for
vulnerability reporting.

## How it works

The filter uses PipeWire's [filter-chain module](https://docs.pipewire.org/page_module_filter_chain.html)
with built-in biquad filters. Device routing is controlled through PipeWire's
PulseAudio compatibility tools. [Easy Effects](https://github.com/wwmm/easyeffects)
is a more advanced alternative if you need compression, convolution, or a large
plugin collection.

Licensed under MIT. SoundCurrent EQ is an independent project and is not
affiliated with FxSound. Its adjustable EQ workflow was informed by
[FxSound's public documentation](https://github.com/fxsound2/fxsound-app/blob/main/docs/COMMAND_LINE_OPTIONS.md);
no FxSound code or artwork is included.
