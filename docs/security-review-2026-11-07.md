# Security review — 7 November 2026

Status: bounded source/package review, not a certification. Draft changes are not installed or released. Windows 10 qualification is still pending.

## Application findings

* **Mitigated: Qt HTTP/2 use-after-free path (CVE-2026-95620).** The updater is the only application QNetworkAccessManager client found. Both applications now explicitly disable HTTP/2 for their small GitHub JSON requests, following [Qt’s mitigation](https://www.qt.io/blog/security-advisory-cve-2026-95620). The injected-reply test asserts the attribute is explicitly present and false. English, French and Arabic analysis/update tests passed locally. This does not patch Qt globally.
* **Open dependency qualification: Cache-Control parser (CVE-2026-76151).** The host has Qt 6.10.2+dfsg-7; [Ubuntu lists Resolute as needing evaluation](https://ubuntu.com/security/CVE-2026-76151). No applicable backport was established from the installed package changelog. An unusually large header from a compromised server could crash an affected client. HTTP/2 disabling does not resolve this separate parser issue. Before release, qualify a patched distro runtime/backport, or prevent remote checks on affected, unqualified runtimes. Windows builds pin and hash-check Qt 6.12.0, outside this advisory’s affected range. Do not replace the host desktop’s shared Qt as an incidental application change.
* **Binary JSON advisory distinguished from text JSON.** The [official CVE-2026-101201 patch](https://download.qt.io/official_releases/qt/6.11/CVE-2026-101201-qtqt5compat-6.11-0001.diff) changes Qt5Compat QBinaryJson::fromBinaryData. These applications parse text JSON with QJsonDocument::fromJson; no Qt5Compat dependency or QBinaryJson call was found. A version match alone is insufficient to label profile import vulnerable to this binary-format flaw.

## Boundaries inspected

Profile imports are limited in size/filter count and validate numeric values. Profile details use plain text; external source links are explicit user actions. The updater restricts release links to the expected HTTPS GitHub repository, caps response bodies and timeouts, and does not silently execute an installer. Body limits do not protect Qt’s earlier HTTP header parser.

Microphone permission defaults denied until availability is known, persists explicit choices, and checks permission in the capture backends as well as disabling UI. Revocation stops capture/analysis/calibration work. Playback analysis is independent. Synthetic tests prove policy/backend behavior; real Windows privacy-indicator and physical hotplug verification remains separate. No claim is made that a software privacy switch overrides OS privacy controls or disables unrelated applications.

## Remaining security qualification

Review other applicable Qt Network advisories, including NTLM response handling, against actual proxy/authentication paths and distribution backports. Recheck the final shipping runtime/plugin inventory, rather than treating every Qt module advisory as applicable. No exhaustive third-party scanner or penetration test was performed. Profile fuzzing, kernel Driver Verifier and production-signing qualification remain additional work; passing functional tests is not proof of absence of vulnerabilities.
