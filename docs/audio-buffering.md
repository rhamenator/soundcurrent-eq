# Audio buffering

Both desktop apps expose **Settings & calibration → Audio buffering** on Linux
and Windows. Select Automatic or 5, 10, 20, 40, 80, 120, or 200 ms, then Apply.
The change is staged until Apply; applying briefly restarts active playback and
microphone processing. Settings are disabled while controls are locked or a
measurement is running. A failed restart restores the previous saved value and
attempts to restart the previous configuration; errors remain visible in the panel.

Automatic leaves Linux's previous scheduling requests unchanged (Studio uses
512/48000; EQ uses the server defaults). Windows Automatic requests 40 ms instead
of the previous fixed 200 ms. Smaller choices can reduce delay but may produce
crackles or dropouts on a busy machine; increase the value if that happens.

These are requested buffer durations, not measured end-to-end latency. Windows
uses shared-mode WASAPI and adjusts to supported engine/device sizes; the worker
poll interval scales from 1 to 5 ms. Linux sends a per-node `node.latency` request
to both playback and capture; it does not force the server's global quantum.
PipeWire may choose a shorter period or be constrained to a longer one. Other
applications, virtual audio routing, resampling, USB devices and headphone
processing can contribute additional delay. Signing a driver does not itself
change any of these buffer durations.

References: [Microsoft low-latency audio](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio),
[PipeWire scheduling properties](https://docs.pipewire.org/1.2/page_man_pipewire-props_7.html).

Tests cover duration units, limits, platform defaults, staged Apply, restart
failure rollback, saved custom values, and propagation to Linux graph properties.
Native Windows CI builds and exercises the installed UI; real hardware audio/video
synchronization still needs a Windows host check, including USB headphones.
