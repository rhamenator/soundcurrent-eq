# Windows signing and release setup

## Current route

SoundCurrent uses its own SYSVAD-derived **kernel virtual audio driver** plus
GPL userspace apps, a route guardian and a privileged driver manager. The driver
has separate MS-PL licensing. It registers only `Root\SOUNDCURRENTVAD` with
service `SoundCurrentVAD`; it does not replace physical-device drivers.

The earlier native APO experiments are historical. Their DLL preflight and
protected-AudioDG deployment questions do not establish readiness of the current
virtual driver. This release requires the actual SoundCurrent INF/SYS/CAT package.

## Owner prerequisites

The owner has Microsoft developer/Store accounts, but reported no signing
certificate or signing-service account. Hardware Developer Program enrollment,
publisher identity and account eligibility have not been verified. No certificate
purchase, enrollment or submission has been performed.

Microsoft requires a valid EV certificate associated with the Hardware Dev Center
account for both attestation and WHCP submissions. A registered Authenticode
certificate may sign an individual submission; SHA-256 is required. See
[driver code-signing requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-reqs).

Microsoft documents attestation for testing, without HLK certification or retail
Windows Update publication. Use the WHCP/HLK route as the public-release plan;
confirm the applicable virtual-audio test requirements before submission. See
[driver signing offerings](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings).
Both pages were rechecked on 2026-10-06 and report an update date of 2026-04-14.

## Concrete handoff

1. The owner confirms the legal publisher identity and Hardware program access,
   then approves a certificate provider and cost. Microsoft lists eligible EV
   providers on its requirements page. Do not purchase or enroll on the owner's
   behalf without that approval. Keep keys in the provider's protected signing
   store, outside source, build archives, command lines and logs.
2. Freeze a source revision and build the existing driver project using the pinned
   SDK/WDK and MSVC configuration. Preserve the MS-PL provenance and licenses.
   Prepare submission files and required test evidence from that exact revision.
3. Submit through the verified Hardware dashboard route. Retrieve the returned
   package and retain its submission identity, hashes and test evidence. An
   attestation-returned package can unblock secured-clone testing; it is not proof
   of production certification or runtime quality.
4. Build and Authenticode-sign the manager from the same release source. Verify
   the signed manager and the returned package. Separately sign the applications,
   guardian and installer before publishing; the current build script checks
   prerequisites but does not perform those signing operations automatically.
5. Run the source acceptance gates in
   [the completion audit](../native/windows/virtual-driver/COMPLETION-AUDIT.md)
   on full independent Windows clones. Require own-driver playback and microphone
   runtime, route/volume restoration, hotplug, update/shared-owner removal,
   elevation and reboot handling, and unchanged physical-driver inventory.
6. Build each installer with `scripts/build-windows.ps1`, supplying `AudioRoute Native`, `QtPrefix`,
   `SignedDriverPackage` and `SignedDriverManager`. The script rejects an untrusted
   manager or a package that fails manager verification before building. Publish
   only after signed installer/runtime checks and matching GPL/MS-PL/Qt source and
   license distribution checks pass.

## What is available now

Both repositories contain reviewable source, ignored unsigned driver artifacts
and unsigned app-only installer prototypes. Their current hashes and verification
scope are recorded in
[verification-2026-10-06.json](../native/windows/virtual-driver/verification-2026-10-06.json).
The unsigned own driver has not been installed. Existing cable fixtures verify
userspace behavior only; they do not satisfy the own-driver runtime gates.

Do not change Secure Boot, signature enforcement, protected-audio settings or
trust policies to work around a failed signature check. No such change was made.
Windows-trusted driver signing and a verified signed manager remain external
prerequisites; the complete goal remains unproven until its runtime gates pass.
