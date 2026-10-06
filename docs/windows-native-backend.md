# Native Windows endpoint processing

Status: planned; not available in 0.7.4 EQ / 0.8.3 Studio.

## Product requirement

Provide processing on selected physical Windows playback and microphone endpoints
using their installed drivers. Leave Windows default endpoints and communications
roles unchanged. No virtual cable is required for this mode. Attach only to devices
the user explicitly enables; newly connected devices need an explicit enrollment
policy. Expose status, enable/disable, Windows sound settings, and component removal
in the application. Keep the existing cable backend as a separately chosen option.

## Technical route and limits

Use a user-mode system-effect Audio Processing Object (APO). Share the existing
framework-independent C++ DSP and versioned parameter model with both applications;
do not load Qt into the Windows audio-engine process. Prefer endpoint correction
placement where supported; inspect existing OEM effects before selecting placement.
Do not overwrite manufacturer effects or claim compatibility with every driver.
Exclusive/ASIO, hardware-offloaded and other bypass paths need separate testing and
clear supported-path reporting. WASAPI loopback captures a copy of already rendered
audio: processing and rendering that copy does not replace the original output.

Installation requires an evaluated device association and signed deployment route.
Microsoft documents INF-based installation; generic association with arbitrary
third-party endpoints remains a feasibility gate, not a solved assumption. Preserve
and restore prior endpoint configuration transactionally, including failed installs,
driver upgrades, disable and uninstall. Never disable Windows signature enforcement.

Realtime processing must use bounded/preallocated buffers and validated parameter
snapshots with safe retirement; no GUI, disk access, blocking IPC, allocations or
logging in APOProcess. Control communication must authenticate/authorize the intended
user/session and endpoint, validate lengths and versions, and default to bypass on
invalid/stale state. Quit must release processing ownership and bypass the resident
component; closing the window can retain it. Coordinate EQ/Studio exclusion in this
shared backend, including background processes and multiple Windows sessions.

## Staged acceptance gates

1. Build a separate APO DLL and host harness; verify float formats/channel layouts,
   silence, in-place/out-of-place buffers, bypass, latency/tails and parity with DSP
   reference output. Start with stereo EQ; keep Studio channel layouts explicit.
2. Demonstrate attachment on a full independent Windows VM clone, using a reversible
   test deployment. Preserve original VMs and normal host music. Verify rollback and
   that default endpoint IDs/roles are unchanged throughout. Do not ship test signing.
3. Add bounded secure control transport, metering, immediate controls, ownership and
   device-disconnect handling; exercise crash/restart and Windows audio-service restart.
4. Test real native USB/onboard/Bluetooth devices, OEM effect coexistence, sample-rate
   changes, Windows volume, unsupported bypass paths and driver-update recovery.
5. Ship signed installation/removal with per-device status, diagnostics and repair.
   Cable-free mode is offered only after these gates pass.

Next implementation task: an unregistered APO DLL plus offline host harness using
existing stereo EQ/enhancement DSP. Establish Windows format negotiation and exact
bypass before touching device configuration.

## Sources checked 2026-10-05

- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects
- https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording

The present Linux backend uses PipeWire filter-chain processing, not VB-CABLE.
