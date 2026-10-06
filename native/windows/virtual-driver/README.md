# SoundCurrent virtual audio driver development

The independently built kernel driver dependency lives in `vendor/` and is
licensed under MS-PL, including derivatives. The surrounding SoundCurrent apps
remain GPL-3.0-only. Driver source is not linked into the application binaries.

## Provenance

Microsoft Windows-driver-samples, commit
`2dc3fd3a0cc84a2933f2194e7ec0871584979071`, `audio/sysvad`,
https://github.com/microsoft/Windows-driver-samples . Original source headers and
license are preserved. SDK/WDK dependencies are pinned to 10.0.28000.2526.

## Current state

This is an adapted development driver, not an installable signed release.
It exposes one `Root\SOUNDCURRENTVAD` render endpoint named SoundCurrent Audio,
with 48 kHz stereo PCM at the kernel interface, no offload, and no hardware
loopback or fabricated microphones. Windows software loopback is the intended
capture source. The SYSVAD sample hardware loopback synthesizes tones, so that
pin is deliberately excluded. Unused upstream sample definitions remain in the
vendor tree for provenance but are not registered by the SoundCurrent INF.
The driver now builds successfully; it has not yet been installed or run. Do not ship its demonstration
APOs or install the upstream sample INFs.

The application bridge now accepts render-endpoint loopback with a separate
physical destination. Its initial input contract is 48 kHz stereo float; it
rejects unsupported formats and self-loop routes. Both apps prefer this route when its interface name is present; legacy cable
routing remains a fallback for existing installations. Playback output lists
exclude the virtual endpoint to prevent self-routing.

## Remaining delivery gates

Implement microphone routing, compile with
MSVC/WDK, validate INF/catalog, acquire Microsoft production driver signing,
implement shared elevated installation/removal and route recovery guardian,
and verify both complete installers on independent Windows VM clones.
No Secure Boot, signature enforcement, or protected-audio settings are changed.

## Build evidence (2026-10-06)

The independent `soundcurrent-win11-dev` clone restored all three pinned
NuGet packages using a signature-verified Microsoft NuGet executable. Visual
Studio's WDK component installation completed successfully on that clone.
The adapted driver and common library built under MSBuild 18.10.1 / MSVC
14.51.36231 / SDK+WDK 10.0.28000.2526 with zero warnings and errors. The
WDK Universal API validator passed. INF verification completed without an
error. Inf2Cat signability checks for Windows 11 22H2, 24H2 and 25H2 passed
with no warnings or errors and generated an unsigned catalog. The binary remains
unsigned, uninstalled, and untested as an active audio driver. No Windows
security settings were changed. Complete native MSVC app/UI tests passed on the clone: EQ 8/8 and Studio 10/10. These do not prove active driver audio or full installer lifecycle behavior.

## Recovery helper development

Playback and microphone route leases start `soundcurrent-route-guardian.exe`
before changing defaults, with an explicit ready handshake. The helper receives
prior role IDs and waits on its parent's pipe; EOF triggers conditional
restoration, including system volume transfer for playback. Normal shutdown
also closes that pipe after the parent restores its routes. The helper is a
separate executable, so mutual-exclusion scans do not mistake it for a second
EQ app. Both copies compile with MinGW C++20. On the independent Windows clone,
both helper builds passed ready-handshake, owner-pipe EOF, unrelated-route
preservation, and malformed-invocation checks. Full application crash tests
with an installed SoundCurrent driver are still required.

## Live loopback diagnostic gap

The new opt-in `--run-loopback` audio diagnostic started successfully and
received capture packets, but measured zero output through the existing
VB-CABLE fixture. A physical-source reverse-route diagnostic also received
packets with zero signal. Source endpoint mute/volume was explicitly checked
and temporarily normalized, then restored. The cause remains unresolved;
no SoundCurrent driver audio success is claimed from these experiments.
The legacy paired-capture path is being checked to distinguish an environment
problem from a loopback implementation issue.

After signing into the clone's console using its source VM's saved credential,
Flat loopback RMS measured 0.014142 and immediate post-gain measured +6.00 dB.
The next -12 dB EQ-cut assertion measured +13.27 dB and failed. The cause is
unresolved; this is partial audio evidence, not a passed live audio suite.
See `verification-2026-10-06.json` for the current verification limits.
