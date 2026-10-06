# Native Windows endpoint processing

Status: first implementation gate completed; native device attachment is not yet
available in EQ 0.7.5 / Studio 0.8.4. The Windows installers still use VB-CABLE.

## Implemented prototype

`native/windows` builds a Qt-free COM/APO DLL, local configuration interface,
processing harness and read-only endpoint/trust audit. Both repositories passed
MSVC x64 native tests for COM identity/unload, initialization, float32 stereo format
negotiation, capacity/connection bounds, silent input, aliased buffers, exact bypass,
immediate gain and bounded profiles. Portable tests also exercise shared-DSP parity,
effect tails, mailbox concurrency and no allocation/free in the processing call.
Existing Linux suites passed (EQ 7/7, Studio 9/9, including spinner acceleration).

Coefficient/headroom preparation now runs outside processing, with fixed snapshots
applied between blocks. Studio retains its existing LowPass filter support; this
first APO adapter deliberately accepts only front stereo float32. Studio's full
multichannel graph, microphone routing and UI cross-process control are subsequent
gates, not claimed by this prototype. The local control interface is not an IPC
mechanism. No DLL registration, endpoint association or device changes were performed.

Build: `powershell -File scripts/build-native-apo.ps1` on a VS2022/MSVC x64 machine.
On Linux, build `native/windows` directly to run the portable processor test.
The unregistered DLL has no Qt dependency and is not included in production setup.

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

Next implementation task: implement authenticated endpoint control and transactional
per-device enrollment/rollback on an independent VM clone, followed by full Studio
graph support. Signed deployment is required before production enrollment is offered.
The read-only trust preflight rejects the present unsigned DLL; Authenticode trust
alone does not establish protected-audio eligibility. See windows-signing.md.

## Sources checked 2026-10-05

- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects
- https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording

The present Linux backend uses PipeWire filter-chain processing, not VB-CABLE.
