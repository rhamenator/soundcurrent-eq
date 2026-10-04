#!/usr/bin/env python3
"""Measure a live PipeWire band change without using the speakers."""

import array
import json
import math
import os
import pathlib
import subprocess
import sys
import tempfile
import time
import wave


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def node_id(name):
    for item in json.loads(run("pw-dump", capture_output=True, text=True).stdout):
        if item.get("info", {}).get("props", {}).get("node.name") == name:
            return item["id"]
    return None


def capture(tone, destination, sink):
    with destination.open("wb") as output:
        recorder = subprocess.Popen(
            ["parec", "-d", sink + ".monitor", "--format=s16le", "--rate=48000", "--channels=2"],
            stdout=output,
            stderr=subprocess.DEVNULL,
        )
        try:
            time.sleep(0.3)
            run("paplay", "-d", "soundcurrent_eq", str(tone))
            time.sleep(0.1)
        finally:
            recorder.terminate()
            recorder.wait(timeout=5)
    samples = array.array("h")
    samples.frombytes(destination.read_bytes())
    audible = [value for value in samples if abs(value) > 4]
    if len(audible) < 1000:
        raise RuntimeError("The test signal did not reach the silent output")
    return math.sqrt(sum(value * value for value in audible) / len(audible))


def channel_levels(destination):
    samples = array.array("h")
    samples.frombytes(destination.read_bytes())
    if len(samples) < 2000:
        raise RuntimeError("The stereo level recording is too short")
    return tuple(math.sqrt(sum(value * value for value in samples[channel::2]) /
                           len(samples[channel::2])) for channel in (0, 1))


def main():
    binary = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "build/soundcurrent-eq").resolve()
    if node_id("soundcurrent_eq") is not None:
        raise RuntimeError("Turn off SoundCurrent EQ before running the audio test")
    sink = f"soundcurrent_test_{os.getpid()}"
    module = None
    pipewire = None
    with tempfile.TemporaryDirectory(prefix="soundcurrent-audio-test-") as directory:
        directory = pathlib.Path(directory)
        try:
            module = run("pactl", "load-module", "module-null-sink", f"sink_name={sink}",
                         "sink_properties=device.description=SoundCurrent_Test_Output",
                         capture_output=True, text=True).stdout.strip()
            config = run(str(binary), "--dump-filter-config", sink,
                         capture_output=True, text=True,
                         env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}).stdout
            (directory / "filter.conf").write_text(config)
            pipewire = subprocess.Popen(["pipewire", "-c", str(directory / "filter.conf")],
                                        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            for _ in range(40):
                eq_id = node_id("soundcurrent_eq")
                if eq_id is not None:
                    break
                if pipewire.poll() is not None:
                    raise RuntimeError("PipeWire filter exited before creating the EQ sink")
                time.sleep(0.1)
            else:
                raise RuntimeError("PipeWire did not create the EQ sink")

            for frequency in (100, 1000):
                with wave.open(str(directory / f"tone-{frequency}.wav"), "wb") as wav:
                    wav.setnchannels(2)
                    wav.setsampwidth(2)
                    wav.setframerate(48000)
                    frame = array.array("h")
                    for n in range(96000):
                        value = round(3000 * math.sin(2 * math.pi * frequency * n / 48000))
                        frame.extend((value, value))
                    wav.writeframes(frame.tobytes())

            monitor_input = capture(directory / "tone-1000.wav", directory / "monitor-input.raw",
                                    "soundcurrent_eq")
            if monitor_input < 100:
                raise RuntimeError("The live level monitor did not receive the EQ input")

            flat_100 = capture(directory / "tone-100.wav", directory / "flat-100.raw", sink)
            flat = capture(directory / "tone-1000.wav", directory / "flat.raw", sink)
            frequencies = (25, 40, 63, 100, 160, 250, 400, 630, 1000, 1600, 2500, 4000, 6300,
                           10000, 16000)
            controls = []
            for channel in ("left", "right"):
                controls.append(f'"{channel}_preamp:Mult" 1.0')
                for index in range(1, 32):
                    frequency = frequencies[index - 1] if index <= len(frequencies) else 1000
                    gain = -12.0 if index == 9 else 0.0
                    controls.extend((f'"{channel}_band_{index}:Freq" {frequency}',
                                     f'"{channel}_band_{index}:Q" 1.0',
                                     f'"{channel}_band_{index}:Gain" {gain}'))
            run("pw-cli", "set-param", str(eq_id), "Props", "{ params = [ " + " ".join(controls) + " ] }",
                stdout=subprocess.DEVNULL)
            cut = capture(directory / "tone-1000.wav", directory / "cut.raw", sink)
            change = 20 * math.log10(cut / flat)
            print(f"1 kHz band response: {change:.1f} dB (expected about -12 dB)")
            if not -14.0 < change < -10.0:
                raise RuntimeError("Equalizer control did not change the audio signal as expected")

            night_controls = run(str(binary), "--dump-preset-controls", "Night Listening",
                                 capture_output=True, text=True,
                                 env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}).stdout
            run("pw-cli", "set-param", str(eq_id), "Props", night_controls, stdout=subprocess.DEVNULL)
            night_100 = capture(directory / "tone-100.wav", directory / "night-100.raw", sink)
            night_change = 20 * math.log10(night_100 / flat_100)
            print(f"Night Listening response at 100 Hz: {night_change:.1f} dB relative to Flat")
            if night_change > -8.0:
                raise RuntimeError("Night Listening did not sufficiently reduce low frequencies")

            flat_controls = run(str(binary), "--dump-preset-controls", "Flat",
                                capture_output=True, text=True,
                                env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}).stdout
            run("pw-cli", "set-param", str(eq_id), "Props", flat_controls, stdout=subprocess.DEVNULL)
            restored_100 = capture(directory / "tone-100.wav", directory / "restored-100.raw", sink)
            restored_change = 20 * math.log10(restored_100 / flat_100)
            if abs(restored_change) > 1.0:
                raise RuntimeError("Flat did not restore the unprocessed signal")

            loudness_controls = run(str(binary), "--dump-preset-controls", "Loudness",
                                    capture_output=True, text=True,
                                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}).stdout
            run("pw-cli", "set-param", str(eq_id), "Props", loudness_controls, stdout=subprocess.DEVNULL)
            loudness_100 = capture(directory / "tone-100.wav", directory / "loudness-100.raw", sink)
            loudness_1000 = capture(directory / "tone-1000.wav", directory / "loudness-1000.raw", sink)
            low_relative = 20 * math.log10(loudness_100 / flat_100)
            mid_relative = 20 * math.log10(loudness_1000 / flat)
            print(f"Loudness bass contour: {low_relative - mid_relative:+.1f} dB versus midrange")
            if low_relative - mid_relative < 3.0:
                raise RuntimeError("Loudness did not emphasize bass over midrange")

            gain_controls = run(str(binary), "--dump-preset-controls", "Flat", "6.0",
                                capture_output=True, text=True,
                                env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}).stdout
            run("pw-cli", "set-param", str(eq_id), "Props", gain_controls, stdout=subprocess.DEVNULL)
            gained = capture(directory / "tone-1000.wav", directory / "gained.raw", sink)
            gain_change = 20 * math.log10(gained / flat)
            print(f"Post-EQ output gain: {gain_change:+.1f} dB (expected +6 dB)")
            if not 5.0 < gain_change < 7.0:
                raise RuntimeError("Output gain did not raise the processed signal")

            for position in (-100, -50, 100):
                balance_controls = run(str(binary), "--dump-preset-controls", "Flat", "0", str(position),
                                       capture_output=True, text=True,
                                       env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}).stdout
                run("pw-cli", "set-param", str(eq_id), "Props", balance_controls,
                    stdout=subprocess.DEVNULL)
                recording = directory / f"balance-{position}.raw"
                capture(directory / "tone-1000.wav", recording, sink)
                left, right = channel_levels(recording)
                expected_ratio = 0.0 if abs(position) == 100 else 0.5
                actual_ratio = right / left if position < 0 else left / right
                if abs(actual_ratio - expected_ratio) > 0.05:
                    raise RuntimeError(f"Balance {position} changed channels by the wrong amount")
                print(f"Balance {position:+d}: left {left:.0f}, right {right:.0f} RMS")
        finally:
            if pipewire is not None:
                pipewire.terminate()
                try:
                    pipewire.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pipewire.kill()
                    pipewire.wait()
            if module is not None:
                run("pactl", "unload-module", module)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Audio response test failed: {error}", file=sys.stderr)
        sys.exit(1)
