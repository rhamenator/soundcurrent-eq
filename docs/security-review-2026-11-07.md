# Security review — 7 November 2026

Status: bounded source/package review, not a certification. Draft changes are not installed or released. Windows 10 qualification is still pending.

## Application findings

* **Mitigated: Qt HTTP/2 use-after-free path (CVE-2026-95620).** The updater is the only application QNetworkAccessManager client found. Both applications now explicitly disable HTTP/2 for their small GitHub JSON requests, following [Qt’s mitigation](https://www.qt.io/blog/security-advisory-cve-2026-95620). The injected-reply test asserts the attribute is explicitly present and false. English, French and Arabic analysis/update tests passed locally. This does not patch Qt globally.
* **Open dependency qualification: Cache-Control parser (CVE-2026-76151).** The host has Qt 6.10.2+dfsg-7; [Ubuntu lists Resolute as needing evaluation](https://ubuntu.com/security/CVE-2026-76151). No applicable backport was established from the installed package changelog. An unusually large header from a compromised server could crash an affected client. HTTP/2 disabling does not resolve this separate parser issue. The new updater refuses real remote checks unless runtime Qt is at least 6.11.3 or in the patched 6.8.10+ LTS line. Local detection and browser downloads remain available. Older distro backports need explicit qualification before relaxing this conservative gate. Windows builds pin and hash-check Qt 6.12.0, outside this advisory’s affected range. Do not replace the host desktop’s shared Qt as an incidental application change.
* **Binary JSON advisory distinguished from text JSON.** The [official CVE-2026-101201 patch](https://download.qt.io/official_releases/qt/6.11/CVE-2026-101201-qtqt5compat-6.11-0001.diff) changes Qt5Compat QBinaryJson::fromBinaryData. These applications parse text JSON with QJsonDocument::fromJson; no Qt5Compat dependency or QBinaryJson call was found. A version match alone is insufficient to label profile import vulnerable to this binary-format flaw.

## Capture-helper lifetime fix

Linux helpers previously set PR_SET_PDEATHSIG without checking whether the parent had already exited between fork and installing the signal. That race could leave a recorder/player helper alive after calibration-worker termination. All Linux QProcess child modifiers now capture the expected parent PID before fork, install the death signal and exit immediately if the parent has already changed (or installing the signal fails). The child callback uses system calls only. The regression fixture exercises both orderings across 40 test-owned process trees without opening an audio device.

## Boundaries inspected

Profile imports are limited in size/filter count and validate numeric values. Profile details use plain text; external source links are explicit user actions. The updater restricts release links to the expected HTTPS GitHub repository, caps response bodies and timeouts, and does not silently execute an installer. Body limits do not protect Qt’s earlier HTTP header parser.

Microphone permission defaults denied until availability is known, persists explicit choices, and checks permission in the capture backends as well as disabling UI. Revocation stops capture/analysis/calibration work. Playback analysis is independent. Synthetic tests prove policy/backend behavior; real Windows privacy-indicator and physical hotplug verification remains separate. No claim is made that a software privacy switch overrides OS privacy controls or disables unrelated applications.

## Sanitizer check of shared profile code

The equipment profile header/implementation are byte-identical in both feature worktrees. The EQ equipment test harness was rebuilt with Clang 21.1.8 using `-fsanitize=address,undefined -fno-omit-frame-pointer`, then run with `QT_QPA_PLATFORM=offscreen ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1`. It exited 0 without sanitizer findings: 1,152 bundled profiles, parser limits, inverse fitting, save/reopen, dirty-editor save-copy and eight invalid cases passed. This is a bounded existing-test run of shared profile code, not coverage-guided fuzzing or a sanitizer qualification of the entire Studio engine. Qt itself was the distribution binary, not rebuilt with instrumentation.

## Third-party cable package

The ordinary Windows installer currently pins VB-CABLE Pack45 to SHA-256 `b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb` and checks executable Authenticode status before launching vendor setup. Its proprietary kernel implementation was not source-audited. On this review date the [vendor download page](https://vb-audio.com/Cable/) advertises Pack46 (November 2026); the downloaded candidate has SHA-256 `bc44e8974191a5a2427d6c979e2dab08dbb09925f3f435dfa2f0ba3e45b6eb90` and an INF dated 31 October 2026, driver version 3.5.2.0. It has not been substituted, executed or qualified in the app installers. A newer package is not evidence that Pack45 has a particular vulnerability; vendor-package refresh/signature/lifecycle qualification remains separate from review of the SoundCurrent native driver.

## Remaining security qualification

[Qt NTLM handling CVE-2026-105946](https://www.qt.io/blog/security-advisory-cve-2026-105946) also affects the old Linux runtime when NTLM authentication is used. No app credentials or authentication handlers were found; the runtime gate avoids relying on assumptions about system proxy behavior. Recheck the final shipping runtime/plugin inventory, rather than treating every Qt module advisory as applicable. No exhaustive third-party scanner or penetration test was performed. Profile fuzzing, kernel Driver Verifier and production-signing qualification remain additional work; passing functional tests is not proof of absence of vulnerabilities.
