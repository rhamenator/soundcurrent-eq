# EQ verification related to Studio issue 22

The affected PC had saved VB-CABLE Windows Volume Control enabled but the live driver flag disabled. Explicit volume repair changed the guard result from 12 to 0. EQ shares this driver and the endpoint-only readiness defect. Its guard now verifies saved/live volume readiness too, with inert regression cases. EQ has no Studio offline session or editing-mode control, so the Studio staging fix is inapplicable.

On the affected Windows PC with signed VB-CABLE 3.3.1.7, Realtek Audio enhancements Off, Secure Boot enabled, and no microphone:

- Cable volume fixture passed.
- Live EQ bridge: Windows endpoint volume attenuation and mute passed. Actual media keys gave -1.01 dB volume-down (endpoint reported -1.60 dB), mute RMS 0 / 0, and restored unmute.
- Output gain +6.00 dB, 1 kHz cut -12.00 dB, bypass 0.00 dB, full-left balance, processing restart and calibration playback passed.
- Live backend enhancement checks: Bass Boost +6.90 dB at 80 Hz, Clarity +4.80 dB at 8 kHz, reset restored dry output.
- Actual EQ UI sliders at 100%, with independent 80 Hz left / 8 kHz right tones: Clarity +0.01380 / +4.78869 dB; Ambience +0.76016 / +2.31325 dB; Surround +2.50485 / +2.49002 dB; Dynamic Boost +9.00629 / +8.99543 dB; Bass Boost +6.89590 / +0.06286 dB. Each slider reset restored dry output. Enable/disable/re-enable/Quit restored all playback roles. The test now locates Quit by stable object name.
- Endpoint roles, levels and mutes matched the pre-test journal after all checks. Both installed helpers reported ready (0).

No separate EQ enhancement-application defect reproduced after repairing volume and disabling Realtek output enhancements. The production UI-to-DSP path already delivers these effects; this change adds end-to-end regression coverage and repairs readiness detection. Realtek's enabled default effects initially distorted gain and channel-isolation measurements; Windows Settings was used to switch enhancements Off per the user's preference. No native driver or hardware-specific gain was changed.

Based on EQ main df7364bde393983f3134cb1d507fdbb6d63cd305. Local VS2026/MSVC 19.51 Release x64, single worker at BelowNormal priority; Qt 6.5.3 SDK with build-only compatibility wrapper for removed MSVC stdext iterator helpers, retaining installed Qt 6.12.0 runtime. See the linked Studio report for setup details. Standard current-SDK CMake builds do not need that old-SDK workaround.

Reproduce with windows-cable-volume-configuration and shared UI checks, then explicitly run `soundcurrent-windows-audio-smoke --run` and `soundcurrent-eq --windows-live-ui-test` with QT_FORCE_STDERR_LOGGING=1, other media paused, both apps quit and active signed-in cable/physical endpoints. Quiet tones are generated and routing/levels are restored. Loopback measurements establish digital transport, not acoustic output. Windows boot persistence and Linux runtime were not retested.

Installed local patched EQ SHA256: 550E80EA0D9C1945E0FE98C80F23D9A2FFC23935F2B81B2DF3FB0C2ED29AABE3. Original published installation and rollback files remain preserved. No new release was published.

**Post-reboot update:** the initial volume persistence failed a Windows reboot. See [the correction and current results](post-reboot/README.md). Revised second-reboot validation remains pending.
