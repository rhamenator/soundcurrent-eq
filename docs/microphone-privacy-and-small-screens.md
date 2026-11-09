# Microphone privacy and smaller displays

## Microphone access

The Microphone setup dialog has a **Disable microphone access** checkbox.
Checked means that the app cannot capture a microphone. Speaker playback and
playback-monitor analysis continue working.

The selected rolling spectrogram advances through silent periods. Analyzer
capture is batched and display work is bounded independently of playback; the
analyzer remains inactive while its tab is hidden.

Before any manual privacy choice, access follows microphone availability: no
connected microphone means disabled; plugging one in makes it available without
needing to restart or press Refresh. A built-in microphone counts as connected.
This makes the device available; it does not turn on Natural mic EQ by itself.

Every manual choice is saved separately by each app. An explicit disable choice
remains disabled after restart or hotplug. An explicit allow choice is retained
while disconnected, but capture is unavailable until a microphone is present.
The disabled checkbox is shown checked while no microphone is available; this
never overwrites a saved allow preference. OS privacy restrictions still apply.

Disabling access stops microphone EQ and microphone analysis, cancels a running
measurement, and disables microphone selection/adjustment and measurement-start
controls. Timers, buffer changes, profile selection and hotplug cannot bypass
the backend permission checks. Playback loopback is distinct from microphone
capture. Offline equipment-profile editing/import remains possible.

An explicitly selected microphone that disappears is retained as an unavailable
selection, rather than silently changing to a different built-in microphone.
Disconnecting the microphone used by a measurement cancels that measurement.
Linux device events and Windows device-change events trigger discovery; the
existing periodic discovery remains a fallback.

Current microphone processing routes to a recording source, not a speaker
monitor bus. This privacy control does not claim to cancel another application's
or external hardware's microphone monitoring. Feedback prevention for a future
application mixer remains a separate routing/interlock requirement.

## Smaller screens and display scaling

Quit is pinned above the tabs, inset from the window edge, with the full app-exit
behavior. Update remains immediately to its left when available. Tab overflow
uses scroll buttons. Studio's persistent output row stacks when labels/fonts
need more room, and its footer has a bounded scroll fallback on very small
logical desktops so it cannot consume the entire application.

EQ band columns and their height use actual control/font sizes, avoiding clipped
frequency buttons and labels. Setup/profile dialogs keep scrollable bodies and
pinned actions; translated forms wrap long rows and narrow action groups can
stack. Window/dialog geometry is clamped to the available logical display and
responds to resize, font and screen changes. Native file pickers/message boxes
retain their platform behavior.

## Verification and limits

- Deterministic policy tests: inverse default, automatic availability before a
  manual choice, saved allow/deny, restart, disconnect/reconnect and fail-closed
  capture gate.
- Main-window self-tests: real checkbox signals/save/reopen, disabled input and
  measurement actions, disconnect does not rewrite a saved allow choice.
- Linux synthetic capture process: denied start never launches capture; revoking
  access stops a running capture without using any physical microphone.
- Analyzer tests: denied microphone sources/actions/profile export, continued
  playback monitoring, resumed selection after permission, hidden-tab gating;
  English, French and Arabic fixtures.
- Windows hardware-free test rejects microphone bridge/recorder access before
  endpoint activation. It does not require an installed driver or microphone.
- Responsive row/dialog tests cover larger fonts, pinned actions, form wrapping
  and scrolling to the last field. Shared UI runs on an 800×600 X11 desktop with
  150% fonts; localization catalogs contain all new strings in all 34 catalogs.
  Structural translation checks do not certify linguistic accuracy.

Physical hotplug/OS-denied microphone indicator observations and actual movement
between monitors with different DPI need hardware qualification. Do not treat
synthetic source tests or offscreen rendering as those observations. Driver
signing/Windows 10 native compatibility remain separate from these UI changes.

Availability is based on the operating system’s capture-device and jack metadata, without opening a microphone to probe it. Some USB/analog adapters cannot detect whether a microphone capsule is plugged into their jack; an available adapter is not proof of physical capsule presence. The explicit Disable microphone access setting remains the reliable way to prevent app capture in that case.
