> Current requirement-by-requirement status: [completion audit](COMPLETION-AUDIT.md). Dated entries below include historical results and superseded pending checks.

# Integrated Windows audio route — implementation status

Updated 2026-10-06. This is a prototype, not a signed installable release.
The objective remains replacement of VB-CABLE in both EQ and Studio, including
microphone processing, convenient setup/removal, and shared-driver updates.

## Evidence completed

- MS-PL SYSVAD-derived render driver builds with WDK, passes universal API and
  INF checks, and produces a signable catalog. Catalog is unsigned; no driver
  was installed and no Windows signing/security setting was changed.
- Both native applications build. Latest CTest: EQ 8/8, Studio 10/10.
- Render-loopback bridge tested using existing VB-CABLE as the source fixture.
  EQ gain/cut/bypass/balance/restart pass. Studio also passes independent channel
  EQ/mute/effects/bypass and rejected-layout preservation. Its missing loopback
  initialization flag was corrected and verified live.
- Both opt-in route tests use the actual production ManagedWindowsRoute class.
  Normal exit and forced owner termination restore all three playback roles
  and all three microphone roles on the independent clone. This verifies the
  recovery mechanism; an interactive desktop-app crash is still a separate gate.
- Shared manager compiles with MinGW/MSVC. Native status, unsigned/incomplete
  package refusal, malformed command and invalid owner SID checks pass.
- Native setup is now wired into NSIS, release build scripts and the in-app
  button (which quits the app before setup). The signed manager and driver
  package are required by the release build script. The wrapper uses HKLM64
  ownership even from 32-bit installer PowerShell.
- Unsigned installer fixtures passed install, in-place update preserving an
  unknown user file, installed UI checks, unsigned-driver refusal, and uninstall
  with no registered driver owner. Physical/third-party audio devices stayed
  unchanged. Signed-driver and shared-owner lifecycle tests remain separate.
- Manager binary inspection confirms a static runtime and System32-only import
  search flags. The requesting user's SID is resolved before UAC.

The JSON evidence record is `verification-2026-10-06.json`. Local ignored
payload archives include the app, guardian and driver manager; they are not
production installers. Tests ran only on `soundcurrent-win11-dev`, a full
independent clone. Linux playback and original VMs were preserved.

## Required remaining work

1. Verify interactive installer page choices, alternate-admin UAC, the in-app
   setup button and restart reminders with a genuinely signed package. Basic
   plumbing, app update/removal and unsigned refusal are implemented and tested.
2. Verify the implemented kernel microphone transport, endpoints and app route
   on a signed driver: simultaneous speaker/mic EQ, clock drift, stream
   teardown, device loss, and silence after stopping the feed.
3. Harden and exercise shared installation/update/removal: journal recovery,
   import/bind failures, last-owner cleanup, reboot, concurrent setup, multiple
   users and alternate administrator approval. Refuse changes while audio apps
   or their route guardians are active. Preserve physical hardware drivers.
4. Test guardian failure, interactive app crash, manual Windows route changes,
   device removal/automatic switching, volume consistency and explicit on/off.
   Check both app exclusion and recognized third-party equalizer exclusion.
5. Obtain the verified Microsoft production driver-signing route and sign the
   privileged manager/release artifacts. Do not install unsigned code by
   weakening Windows protections. Driver runtime acceptance is unproven until
   a signed package is installed on an independent clone.
6. Exercise real driver playback/capture, both complete installers, upgrade and
   uninstall scenarios on independent clones. Package licenses/source/notices,
   hashes and the final evidence. Do not claim a completed release before these
   gates pass.

Next implementation task: exercise guardian failure and harden the shared
driver journal recovery/update lifecycle while signed runtime testing is pending.

## Microphone queue feasibility evidence (2026-10-06)

The adapter transport header now has a portable test registered as
`microphone-transport` in CTest. The standalone Linux C++20 test passed with
AddressSanitizer/UndefinedBehaviorSanitizer and separately ThreadSanitizer.
It covers bounded overflow, silence on underflow and stopped producer,
consumer-open discard, producer restart, stereo sample ordering, and 200,000
concurrent frames across ring wraps. No sanitizer findings were reported.
This proves only the portable queue behavior; kernel endpoint wiring, WDK
validation of that wiring, clock drift and signed-driver runtime remain pending.

Adapter integration in progress: `IAdapterCommon` now exposes microphone
producer state, consumer start, PCM write and PCM read methods. Each
`CAdapterCommon` owns its transport rather than using a global FIFO. Input
chunks require whole stereo PCM16 frames and aligned memory; invalid read
chunks are silenced. Dedicated feed/capture device enum values are defined,
and render classification includes the feed. These changes are not yet WDK
built. Endpoint descriptors, INF registration and stream callback/state
wiring are still required before this can carry microphone audio.

## Microphone endpoint build evidence (2026-10-06)

Both source trees now define `SoundCurrent Microphone Feed` (render) and
`SoundCurrent Microphone` (capture), stereo PCM16 at 48 kHz, with one kernel
stream per direction. Capture filter installation is enabled. The stream
callbacks transfer PCM through the adapter queue rather than generating sine
audio for this capture endpoint. Producer RUN/PAUSE/STOP controls invalidate
stale audio; direct RUN-to-STOP cancels the timer and flushes queued DPCs.
The feed consumes DMA audio even with sample file logging disabled.

The updated source compiled on the full independent `soundcurrent-win11-dev`
clone using WDK 10.0.28000.2526: zero warnings/errors, Universal API validation
passed. Inf2Cat signability and catalog generation passed with no warnings or
errors. The resulting SYS is 129024 bytes and remains unsigned/uninstalled.
Application endpoint discovery/routing still needs these new microphone
identities. Clock drift, stream teardown and real signed-driver audio
acceptance remain unproven. Previous baseline artifacts are not this build.

Application integration now distinguishes the three managed interface names.
All managed endpoints are excluded from physical device selectors; only
`SoundCurrent Audio` is selected for speaker loopback. Microphone route
discovery prefers the dedicated feed/capture pair, while preserving legacy
pairs for existing configurations. The microphone UI describes the managed
route and no longer requires a second cable. The portable endpoint-identity
test passed; full native application build/CTest verification is in progress.
Actual signed-driver endpoint discovery and audio remain unproven.

Native Windows application verification completed on the independent clone:
soundcurrent-eq built and all 10/10 CTest tests passed, including the
new endpoint identity and microphone queue tests. This does not establish
real microphone routing through the unsigned/uninstalled kernel driver.

Stream teardown hardening: timer deletion/callback quiescence now precedes
miniport release and all stream resource freeing. Microphone producer state
is stopped after quiescence. Feed and capture processing after a stalled
callback is bounded to the latest DMA buffer; older overwritten wraps are
skipped rather than replayed. The updated WDK build passed with zero warnings
or errors on the independent clone. Runtime teardown validation is pending.

## Guardian failure recovery (2026-10-06)

`ManagedWindowsRoute` now releases its Windows route lease when QProcess
reports that its guardian exited. The owner need not exit to restore routes.
The native route smoke test passed six cases for each product on the
independent clone: normal owner exit, forced owner termination, and forced
guardian termination, each for playback and microphone. All three Windows
roles were restored. Guardian failure tests keep the route owner alive and
terminate only the enumerated guardian whose parent is that test process.
These checks use existing virtual/physical endpoint fixtures, not the new
unsigned kernel driver, and do not verify real-time audio or the full UI
bridge shutdown timer.

Shared-driver recovery hardening: managers now register and flush app
ownership before Driver Store import/device binding, and flush the OEM INF
package journal before binding. Failed installs retain recovery ownership.
Both native managers built on the independent clone. This narrows recovery
gaps but does not resolve the import-to-journal interruption window or prove
signed install/update/uninstall behavior; those remain required.

Import retry now uses Windows NOOVERWRITE matching and accepts an existing
published OEM INF identity on ERROR_FILE_EXISTS. Both native managers built.
This supports retrying the same signed installer; autonomous recovery without
that installer still needs durable import intent/staged package retention.
Signed runtime fault injection is pending.

Durable PendingImport recovery is now coded in both managers, superseding
the earlier missing-intent gap. Verified staging is retained through import
and OEM journal failure, and subsequent owned setup/removal resumes the
import and journals the published identity. Native builds passed. Runtime
fault injection requires a signed package; orphan staging after intent
retirement and final lifecycle/reboot acceptance still need work.

## Recoverable staging cleanup (2026-10-06)

Both managers persist a cleanup phase after package journaling, retain it
through binding, and retire it only after deleting known staging files.
Native builds passed. Independent-clone synthetic tests passed for partial
cleanup, unknown-file preservation, and a missing stage after interrupted
retirement; all MEDIA device IDs/services/status/error codes were unchanged.
Signed package import/bind/update/removal fault injection is still pending.

Shared ownership synthetic integration checks passed on the independent
clone: releasing EQ ownership retained a different SID Studio owner; repeated
unowned EQ removal was harmless; a held global transaction mutex caused a
bounded ~15 second refusal without mutation; last-owner removal retired the
remaining record. Physical/third-party audio inventory remained unchanged.
No driver was imported. Also hardened process enumeration to fail closed on
enumeration errors; that latest edit awaits native rebuilding. Inspection
found the desktop guard is per-user and needs global cross-user exclusion
to prevent simultaneous starts or app startup during setup.

Cross-session exclusion now uses a shared global Windows mutex in both
desktop guards and driver managers. It is held for app/setup lifetime, with
a common DACL allowing synchronization/modify access across users. Existing
per-user file guards remain for compatibility; Linux behavior is unchanged.
The Windows manager and guard-test targets built; EQ guard tests passed for
distinct runtime directories, owner crash/release, and setup refusal while
the app gate is held. Studio passed the same checks. Actual separate
Windows login sessions and complete desktop rebuilds remain pending.

Actual cross-user gate experiment: a temporary standard local account and
scoped ProgramData test payload were created on the independent clone, then
removed after each attempt. Alternate-credential process launching from SSH
service session did not provide a usable child result; the explicit-result
wrapper exited without creating its result file. This does not prove gate
behavior under separate users. No Windows security setting was relaxed.
Next verification should use a scheduled batch task under the temporary
identity instead of the SSH service session process-launch mechanism.

Cross-user scheduled-task experiment remains inconclusive. S4U registration
was refused; password-backed registration succeeded using an ephemeral
standard-user credential kept in memory, but both CIM and COM run requests
left the task Ready with LastTaskResult 267011 (never run) and no result file.
Task power conditions were adjusted only on the temporary task. Temporary
tasks/accounts/payloads were removed; no security/signing setting changed.
This is a VM execution-mechanism gap, not proof of cross-user exclusion.
Next useful work is completing desktop builds and testing remaining route
behaviors while separate-user execution is diagnosed.

Desktop rebuild regression check: EQ compiled with the global gate, but
shared-ui timed out twice at the unchanged 45-second limit; the first suite
passed 9/10 and stopped before Studio rebuilt. Several ordinary tests also
ran unusually slowly. Clone memory is ample (~13 GiB free of 16 GiB), so
RAM pressure is not established. An offscreen font-directory diagnostic rerun
is in progress. New unowned-default preservation cases are coded in the
route smoke test but have not yet been built/run. Treat latest desktop
verification as failed/incomplete until the timeout is resolved.

The explicit Windows Fonts directory diagnostic also timed out at 45 seconds;
font configuration does not establish a fix. Added test-only construction
progress messages to both main.cpp UI self-tests to locate the stall after
rebuilding. Those diagnostic edits are not yet built. Latest desktop test
gate remains failed; no active build/test handles remain from this attempt.

Full desktop verification rerun completed: current soundcurrent-eq source built,
all 10/10 CTest checks passed on the independent clone. UI test-only
construction markers were included. EQ shared-ui ran in ~5 s and Studio
~2.4 s; prior 45 s timeouts remain recorded as intermittent unexplained
observations, not as a demonstrated product fix. Expanded route smoke
checks are running separately against the current built code.

Expanded route smoke checks passed 8/8 cases for each product: normal owner
exit, forced owner exit, guardian failure with owner alive, and preserving
unowned defaults despite conflicting proposed restore IDs, each for playback
and capture. These use the real production lease/guardian and existing
endpoint fixtures; they do not establish a full Windows Settings interaction
or new signed-driver audio. No build/test handles remain live from this run.

## Refreshed installer fixtures (2026-10-06)

Current native app/manager/guardian payloads and microphone-enabled unsigned
kernel package were rebuilt into both ignored prototype installers. The
independent-clone lifecycle script passed installation, in-place update,
installed UI checks, unsigned-driver refusal, unowned-driver removal and
known payload cleanup. Unknown user files were preserved; all physical and
third-party MEDIA device identities/services/status/error codes stayed
unchanged. Artifact SHA-256 values and sizes are in the JSON evidence ledger.
Both products share the same unsigned kernel-package archive. These are
unsigned test fixtures, not signed distributable releases. Signed driver
installation, actual kernel audio and owned shared-driver lifecycle remain
required; the verified production signing route remains external.

## Windows volume handoff (2026-10-06)

New opt-in `soundcurrent-windows-volume-smoke --run` passed for both products
on the independent clone. It uses the real ManagedWindowsRoute and
WindowsBridge with existing virtual/physical endpoint fixtures, generating
no test signal. Physical 0.71 scalar/unmuted was copied to the owned endpoint;
the active physical output was verified at 1.0/unmuted; changing the owned
endpoint to 0.53/muted was transferred back on bridge/route stop. RAII restored
the fixture volumes afterward. This confirms endpoint control-state handoff,
not measured acoustic/sample-level loudness through the unsigned new kernel
driver. New kernel audio and crash-time volume acceptance remain pending.

### Forced termination volume recovery — 2026-10-06

Both products rebuilt and passed the opt-in Windows volume smoke test on the independent `soundcurrent-win11-dev` clone. The test starts the actual managed route and userspace bridge, changes the virtual endpoint to 42% and unmuted, then forcibly terminates its owner. The guardian restores all three output roles and transfers the latest volume/mute state to the physical endpoint. The normal-stop volume test also passed. Fixture volumes are restored after testing. These checks use existing virtual/physical endpoints without a generated test signal; they do not establish acoustic gain or runtime correctness of the unsigned SoundCurrent kernel driver, which remains uninstalled.

### Removal persistence and restart reporting — 2026-10-06

Both managers now explicitly flush owner release and every retired package-journal entry before reporting removal success. Own-device removal inspects `DI_NEEDREBOOT`/`DI_NEEDRESTART` and reports 3010 when required; the setup wrapper already handles that code. [Microsoft documents these restart flags](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/ns-setupapi-sp_devinstall_params_w). Both native managers rebuilt successfully in the independent clone. Shared-owner/transaction tests, recovery after an already-deleted INF with a surviving journal entry, staged cleanup recovery, and unsigned/malformed package rejection passed. Audio device inventory was unchanged and no driver was imported. Sudden-power-loss durability and real signed-device removal/reboot remain unverified. Earlier unsigned installer prototypes contain the preceding manager revision; these source/build results do not update their recorded hashes.

### Hot-unplug fallback revision — 2026-10-06

Both Windows apps now allow the output-selection refresh to attempt a fallback when the old physical endpoint disappears and its bridge has stopped, provided the managed route guardian remains healthy. Previously the inactive-bridge check disabled processing before fallback selection. A missing/unhealthy guardian still stops processing. Both native desktop builds passed. UI verification is running through CTest (host exec session 53619); the direct PowerShell GUI invocation in the build helper did not reliably wait for test completion and is not an exit-status gate. Live PnP reconnection and signed-kernel audio remain pending. Existing installer prototypes precede this revision.

The hot-unplug revision UI gate subsequently passed through CTest for both products (EQ 5.29 s, Studio 2.95 s). The initial CTest run lacked the deployed offscreen plugin search path; after setting the package plugin/platform paths and Windows font directory, both checks passed. These UI checks do not exercise PnP reconnection.

### Guardian-loss shutdown ordering — 2026-10-06

Managed routes now accept a bridge-stop callback. Both playback and microphone engines provide it so helper failure stops their audio worker before restoring default routes and playback volume. Normal shutdown already uses that ordering. The callback runs only while a route exists; normal route destruction does not invoke it again. The expanded route smoke test checks callback execution while all three defaults still refer to the owned endpoint, then checks restoration with the owner still alive. Build/test results are recorded in the verification JSON. The route fixture uses existing virtual/physical endpoints; a full bridge/volume fault injection and new signed-kernel runtime remain pending. Earlier installer prototypes precede this revision.

### Live bridge guardian fault/volume verification — 2026-10-06

Both products rebuilt and passed the extended volume smoke test in the independent clone. With the actual userspace bridge running, the test forcibly terminates only its own guardian while keeping the owner alive. The stop callback shuts down the bridge, the route becomes unhealthy, all three output defaults are restored, and the latest virtual-endpoint volume/mute is transferred to the physical endpoint. Both 46%/muted and 100%/unmuted were checked, starting from a previously muted physical endpoint; values remained correct after route destruction. Normal-stop and owner-crash cases also passed. Fixture volumes are restored on exit. No test signal is generated. The fixture uses existing virtual endpoints and does not validate the uninstalled SoundCurrent kernel driver or measured acoustic gain.

### Partial volume-startup rollback — 2026-10-06

Both Windows audio implementations now attempt to restore the original physical scalar/mute if the output-volume lease constructor fails after a partial change. C++ does not invoke the destructor for a failed constructor. Restoration is best effort when hardware disappears. Both native audio libraries and linked volume smoke targets rebuilt and passed normal-stop, owner-crash, and live guardian-failure regression cases. A forced endpoint-volume API failure was not injected, so that rollback branch is implemented but not directly verified. Desktop binaries need relinking and existing installer prototypes precede this change.

### Current recovery builds and prototypes refreshed — 2026-10-06

Both desktop applications were relinked with the current Windows audio library. Full Windows CTest passed (EQ 10/10, Studio 12/12), including shared UI. Payload archives and unsigned installer prototypes were regenerated with current desktop, guardian and manager binaries; current sizes and SHA-256 hashes are in the verification JSON. Both refreshed installers passed the independent-clone lifecycle test: silent app install, in-place update with unknown-file preservation, installed UI, refusal of the unsigned driver package, unowned-driver uninstall and known-payload cleanup. Physical and third-party audio inventory was unchanged. This supersedes earlier statements that prototypes precede the recovery revisions. No SoundCurrent kernel driver was installed; these remain unsigned review/test fixtures, not signed distributable releases.

### Live window On/Off/Quit route verification — 2026-10-06

Added an opt-in `--windows-live-ui-test` desktop test, intended only for an independent Windows clone. It uses temporary INI settings and the actual ProcessingGuard, refreshes devices through the window control, enables processing through the real On/Off checkbox, verifies all three owned output roles, disables and verifies restoration, re-enables, then clicks the actual Quit app button. After the event loop and window destruction it verifies the original roles again. Both current desktop builds passed with the real userspace bridge and existing virtual/physical fixture endpoints. No generated audio test signal or new kernel installation is involved. Automatic switching and actual device hot-plug remain pending. The previously refreshed prototype installers do not include this newly added test entry point; their production recovery changes remain current.

### Two-output USB hotplug fixture — 2026-10-06

The independent clone originally exposed only one physical output. A runtime-only QEMU `usb-audio` device (`sc-usb-audio-fixture`, audio backend `audio1`, bus `usb.0`, port 10) was added through QMP after libvirt rejected live sound-device attachment without changing the VM. Windows recognized it with native `usbaudio`, status OK, providing a second physical speaker endpoint. Both current apps passed their live On/Off/Quit route test with the two-output fixture present; those tests do not separately assert which physical endpoint was selected. The temporary device is retained for the next unplug/replug test and is absent from persistent domain XML. Cleanup requires QMP `device_del` for that exact fixture ID and checking qtree/active guest endpoint inventory. The host audio, original VMs and unsigned SoundCurrent kernel installation remain untouched. Actual unplug fallback and replug selection are still pending.

### Actual USB unplug/replug experiment — 2026-10-06

EQ passed the actual runtime USB removal/re-add sequence with the live desktop controls and userspace bridge: explicit USB selection, automatic onboard fallback while remaining enabled, then automatic USB selection when reconnected. All three owned output roles were checked after the UI reported each completed switch; disabling released the owned roles. The first test used an overly immediate role assertion during Windows reconnection and was revised to check completed switches within a bounded timeout. Studio started on USB but did not report fallback within the host coordinator timeout; the coordinator restored the temporary USB device and the app exited through its bounded timeout. Studio unplug/replug remains unverified. Timeout diagnostics now include stage, displayed status and enumerated outputs, but that diagnostic revision needs rebuilding. The temporary runtime USB fixture remains attached for reproduction. No SoundCurrent kernel driver or host audio changes were involved.

### Studio live-monitor unplug/replug verification — 2026-10-06

The Studio timeout was traced to the test constructing `MainWindow(false)`, which intentionally leaves its device polling timer off for offline preview. The hotplug test now uses `MainWindow(true)` to exercise normal live monitoring; diagnostic timeout messages retain stage/status/device information. Studio rebuilt, passed its live control regression, then passed actual USB selection, runtime USB disconnection, automatic onboard fallback while remaining enabled, reconnection and automatic USB return, completed-switch checks of all three managed roles, and disable releasing the route. EQ passed its earlier actual switching run. This establishes userspace switching with existing virtual/physical fixtures, not the uninstalled unsigned SoundCurrent kernel. The temporary USB device was subsequently deleted; it is absent from QEMU qtree and present guest endpoint enumeration, and original High Definition Audio/VB fixture MEDIA devices remain OK. Host audio and original VMs were not changed.

### Package identity preflight — 2026-10-06

Both managers now parse the INF with SetupAPI before trust verification/import and require the own MEDIA class/GUID, catalog name, single manufacturer/model mapping for the exact Root hardware ID and own service mapping. This prevents a differently identified package from reaching Driver Store import merely because it carries a valid signature. Both native builds and extended read-only tests passed: the genuine unsigned package reaches the signature rejection gate, while changed hardware ID, catalog, service and an added model are rejected at the identity gate. No import or device mutation was performed. A valid signed package remains unavailable. Existing unsigned installer prototypes contain the previous manager revision and need refreshing before claiming this check for their bundled helper.

### Identity/hotplug review artifact refresh — 2026-10-06

Both current Windows desktops and managers rebuilt with the package identity gate and latest opt-in live UI/hotplug tests. CTest passed EQ 10/10 and Studio 12/12. Payload archives and unsigned prototypes were refreshed, their current SHA-256/size records updated, and both prototypes passed app-only install/update/installed UI/unsigned-driver refusal/unowned uninstall checks. Physical/third-party audio inventory stayed unchanged. This supersedes earlier prototype-revision warnings for these changes. The completion audit now separates demonstrated userspace behavior, remaining live-user tests, and signed own-kernel/lifecycle prerequisites.

### Direct physical selection and strict GUI exit gate — 2026-10-06

An actual Windows all-role default change to the physical destination reproduced a stale On state in both apps. Their Windows refresh now turns processing off when Windows selects the current physical destination directly, releasing the bridge and volume lease while preserving that output choice. Both rebuilt apps passed the test with explicit completion marker and native exit code 0. The older PowerShell Start-Process wrapper could print PASS with a null/unreliable exit value; its manual-test PASS messages were explicitly rejected after captured failure diagnostics. A strict System.Diagnostics.Process runner now captures streams, rejects missing/nonzero exit codes, and requires the manual-test completion marker. Both prior On/Off/Quit modes were revalidated with that runner (four total cases, all native exit 0). The opt-in runner is saved as tests/windows_live_ui_routes.ps1. Existing prototypes precede the new direct-selection fix. Earlier hotplug phase assertions passed, but final process exit should be revalidated with the strict runner. This test changes actual defaults through endpoint APIs; it does not drive the Windows Settings window or install the unsigned own kernel.

### Strict hotplug exit verification — 2026-10-06

Both current desktop builds, including the direct-physical-output disable fix, passed a fresh runtime USB removal/re-add sequence. The coordinator now uses native System.Diagnostics.Process with retained process state and requires a non-null exit code of 0 plus the final reconnected marker. Actual USB selection, onboard fallback and automatic USB return remained correct; final route-release checks also completed. This supersedes the earlier permissive wrapper for the hotplug test. A parameterized guest launcher is saved as tests/windows_live_hotplug.ps1; the equivalent strict coordinator was exercised, while that standalone parameterized script was not separately run. The temporary runtime USB fixture was removed and confirmed absent from qtree/present guest endpoints; original MEDIA devices remain OK. No own kernel installation or host audio change occurred.

### Whole desktop process forced termination — 2026-10-06

Both actual desktop executable processes passed forced termination with live MainWindow/monitoring and the real userspace bridge. The opt-in tests/windows_desktop_crash.ps1 snapshots all three original output roles using newly rebuilt read-only route utility modes, starts the desktop with the two-output fixture, waits for its actual ready/owned route and identifies its guardian by parent PID, then forcibly terminates the entire app (native exit -1). All three original defaults were restored and the app guardian exited. This uses an offscreen Qt window, not a visible Windows Settings interaction; volume was not separately measured in this test. The existing owner-crash volume fixtures remain the volume evidence. The temporary USB device was removed and confirmed absent from qtree/present guest endpoints; original MEDIA devices remain OK. No own unsigned kernel was installed and host/original VM audio was untouched.

### Actual EQ/Studio executable exclusion — 2026-10-06

The opt-in tests/windows_desktop_exclusion.ps1 passed both actual desktop executables in both directions on the independent clone. An owner starts its live MainWindow/bridge and managed route; the other executable must exit 1 with the specific Audio session busy diagnostic. After forcibly terminating the owner and waiting for its guardian to exit, the contender must complete a real live route test with native exit 0 and explicit PASS marker. Both directions passed using retained native Process handles. This proves actual app exclusion/recovery for the same Windows user/session, not different login identities or all third-party equalizers. The temporary USB fixture was removed and verified absent; original MEDIA devices remain OK. No own kernel installation or host/original VM changes were made.

### Native alternate-user launch investigation — 2026-10-06

A retained native Process launcher with a temporary standard-user credential/profile failed during alternate-process initialization (-1073741502), before the Qt processing guard could run. A new Qt-free static-runtime helper using the exact production WindowsAudioSessionGate built for both products and failed at the same initialization stage when launched under the alternate user. This rules out claiming gate verification from either launch and does not establish that Qt is the cause. No existing desktop/window-station/security policy was loosened. Cleanup was separately verified: no temporary test accounts, profiles, fixture directories or guard processes remain. The next safe investigation is actual standard-user token access to the production mutex under impersonation; actual alternate-user process/session exclusion remains unverified. The opt-in helper is tests/windows_session_gate_smoke.cpp and is not part of CTest or a product runtime requirement.

### Actual standard-user token gate verification — 2026-10-06

The opt-in tests/windows_gate_token_access.ps1 passed for both production-gate builds using a temporary standard user and verified effective impersonated SID. The token opens the actual production-created global mutex with required synchronize/modify rights, receives WAIT_TIMEOUT while the original native owner holds it, acquires it after owner termination, excludes an original-user native contender while holding it, and releases it under the standard-user token so the original user can recover. The Qt-free native owner uses the exact WindowsAudioSessionGate class. The first harness attempted to launch the original-user executable while impersonating and hit access denial; the successful revision reverts the effective token for that launch, then re-impersonates for mutex release. No existing desktop/window-station/security permissions were weakened. The temporary account was removed and a separate audit confirmed no temporary test accounts/profiles/folders/processes remain. This proves real token/ACL/contended-mutex behavior, not alternate-user GUI startup or separate interactive sessions. Those launch scenarios remain unverified.

### Different physical Windows choice — 2026-10-06

The two-output clone test reproduced stale On state when an app-pinned output differed from a new direct Windows physical-output choice. Both Windows refresh implementations now disable processing for that case while retaining unplug fallback and automatic-follow switching. Both current MSVC desktop builds passed the saved `windows_live_ui_routes.ps1 -Run -TwoPhysicalOutputs` runner: six cases total covering On/Off/Quit, same physical selection and another physical selection, all native exit 0 with required manual completion markers. Both then passed runtime USB removal/onboard fallback/re-add with strict native exit 0 and reconnected markers. These offscreen live-window tests change real endpoint defaults through APIs and use the existing userspace cable fixture; they do not establish own-kernel correctness or physical acoustic gain. Current ignored installer prototypes still precede the direct-output fixes.

### Current review installer refresh and license-copy correction — 2026-10-06

Both current MSVC desktops rebuilt; CTest passed EQ 10/10 and Studio 12/12. Review payload archives and unsigned NSIS prototypes now include both direct-output selection fixes. Packaging review found that the Qt notice-copy rule filtered by filename and omitted SPDX-named LICENSES texts, including GPL, LGPL and BSD. Both production scripts now retain all files under LICENSES. The exact Qt 6.12.0 source archive checksum was verified and all 38 LICENSES texts were added to the deployed review payloads. Rebuilt prototypes passed app install, in-place update preserving unknown files, installed UI, required license files, unsigned-driver refusal, unowned-driver uninstall and known-payload cleanup. The saved lifecycle runner now retains native Process handles, captures streams and rejects missing/nonzero exit codes. MEDIA device identities/services/status remained unchanged. Current SHA-256/size records supersede earlier artifact revision warnings. These prototypes remain unsigned; no own kernel was installed or signed production release builder run.

### Live Windows process discovery and enumeration errors — 2026-10-06

Both guards now refuse processing if Toolhelp enumeration ends with an unexpected error, rather than silently allowing it. The exact production discovery passed eight native process probes in the independent clone: harmless renamed Windows cmd processes under FxSound.exe and Peace.exe identities were detected in both builds; an unrelated process and post-cleanup state were allowed. The opt-in script is tests/windows_live_process_conflict.ps1. This does not install/run either third-party equalizer or establish full live-window shutdown; the enumeration failure branch was not injected. Both desktops rebuilt, CTest passed EQ 10/10 and Studio 12/12, and refreshed unsigned installer prototypes passed app-only install/update/installed UI/license checks/unsigned-driver refusal/unowned uninstall. MEDIA inventory remained unchanged. Current artifact hashes are recorded in the verification JSON; own signed-kernel gates remain pending.

### Conflict appearing during ordinary desktop processing — 2026-10-06

Both current MSVC desktops passed four cases through ordinary startup, instance/processing guards and the production two-second conflict monitor. Harmless FxSound.exe/Peace.exe processes were launched only after the live window acquired all three output roles. Before dismissing the warning, opt-in hooks verified power Off and release of the managed route. The coordinator required the matching diagnostic, explicit pre-dialog marker and native exit zero; original output roles were restored and own guardians exited. Settings were isolated; windows were offscreen. The opt-in hooks dismiss the fixture dialog without replacing conflict detection/shutdown. Tests are saved as tests/windows_live_window_conflict.ps1 and src/windows_live_conflict_test.inc. Fixtures were cleaned up. No third-party equalizer software or own unsigned kernel was installed. Actual third-party audio behavior and signed own-driver runtime remain unverified. Existing unsigned installer prototypes precede these test hooks; their previously verified production guard/routing fixes remain included.

### Signing handoff aligned with the implemented route — 2026-10-06

Both signing guides now describe the implemented kernel virtual driver and identify earlier APO experiments as historical. Official Microsoft code-signing requirements and signing offerings were rechecked: EV association and submission requirements remain documented, with attestation described for testing. The concrete handoff separates owner identity/enrollment/provider approval, exact-source submission and returned package evidence, manager signing, signed runtime/lifecycle acceptance, installer generation and release/source obligations. The existing release script was inspected: it requires trusted manager/package inputs before building and does not itself perform signing. No signing credentials, enrollment, purchase, submission or security-policy changes were made.

### Manual physical-output volume/mute preservation — 2026-10-06

The live same-output test reproduced a real cleanup bug: after Windows selected the physical destination and the user set 58% unmuted, shutdown preserved 58% but restored the old mute. Both OutputVolumeLease destructors now restore original mute only while the physical level still matches the lease unity setting, preserving mute alongside a changed physical level. Both desktop builds passed four same/different-output cases with actual endpoint measurements and strict GUI exit/completion gates. Fixture levels were saved/restored; no signal was generated. Existing normal stop, forced owner crash and guardian-failure volume/mute/all-role tests also passed for both current libraries. The opt-in helper gained read/set fixture-level modes, and the saved coordinator is tests/windows_manual_output_volume.ps1. Current payloads/prototypes include this fix and the ordinary live-window conflict hooks; CTest passed EQ 10/10 and Studio 12/12, and both refreshed unsigned prototypes passed app-only lifecycle with unchanged MEDIA inventory. Temporary USB fixture removal was verified. Current hashes supersede prior prototype-revision caveats. This remains endpoint control-state proof using the existing cable fixture, not acoustic/sample gain or own signed-kernel validation.
