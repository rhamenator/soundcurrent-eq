# SoundCurrent EQ

A simple, native C++ desktop equalizer for Ubuntu's PipeWire audio stack. It
provides nine frequency bands, listening presets, saved custom presets, and an
output dropdown with an **Automatic** option that follows connected devices.

![SoundCurrent EQ desktop window](docs/screenshot.png)

SoundCurrent EQ creates a PipeWire virtual sink while it is on. It routes that
sink to the selected physical output and restores the normal output when you
turn it off or close the app. The actual audio processing is performed by
PipeWire's built-in filters, not by the graphical interface.

## Install on Ubuntu

1. Download the `.deb` from the [latest release](https://github.com/rhamenator/soundcurrent-eq/releases/latest).
2. Open it in Ubuntu's package installer, or run:

   ```bash
   sudo apt install ./soundcurrent-eq_*.deb
   ```

3. Launch **SoundCurrent EQ** from the app menu. Choose **Automatic** or a
   specific output device, then turn on the equalizer.

The package declares its dependencies so `apt` installs the required Qt and
PipeWire tools. Releases include a `.sha256` checksum file. The app currently
targets 64-bit Ubuntu 24.04 LTS and newer with PipeWire audio.

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
- Move the nine sliders from −12 to +12 dB. Positive boosts automatically lower
  the preamp to leave headroom. Changes apply without restarting the filter.
- Pick a built-in preset or save your own. Custom presets live in your user
  configuration directory.
- Closing the window turns off processing and restores normal output. The app
  does not need to remain installed as an audio service.

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

## Verify audio routing

```bash
./build/soundcurrent-eq --self-test
```

This briefly creates the EQ sink, changes its controls, and checks that the
original default output is restored. It uses the currently selected physical
output and does not play a test sound.

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
affiliated with FxSound.
