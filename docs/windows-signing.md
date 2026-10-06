# Native Windows signing and release setup

The owner has a Microsoft developer account and an unused Microsoft Store account,
but no current signing certificate or
signing-service account, and requested that setup be included in the plan. Account
type and Hardware Developer Program enrollment have not been verified. No certificate purchase, account enrollment, trust
store change or Windows security downgrade is authorized by that answer.

1. Confirm the APO deployment model and protected-audio requirements with current
   Microsoft guidance before selecting a certificate/service. Ordinary installer
   Authenticode signing does not itself prove APO protected-process compatibility.
2. Determine the publisher identity and obtain the signing credentials required by
   the confirmed route. Keep private keys in a hardware/cloud signing store, outside
   repositories, build archives, command lines and logs.
3. If driver-package submission is required, establish Hardware Developer Program
   enrollment and the applicable EV certificate association. Evaluate current
   WHCP/HLK versus attestation eligibility; do not assume historical policies apply.
4. Build the supported APO/INF/catalog package, run the required audio tests, submit
   through the approved route, timestamp and verify the returned binaries/package.
5. Verify protected/unprotected playback, native driver coexistence, default-output
   preservation, device updates, clean disable/uninstall and rollback on independent
   VM clones and physical devices before enabling native mode in installers.

The prototype endpoint-audit tool performs an offline Authenticode preflight and
returns exit code 2 for an unsigned/untrusted DLL. It never modifies endpoints or
installs certificates. Trust acceptance is one prerequisite, not a full certification
result. A failed check must not be worked around by disabling protected AudioDG,
Secure Boot, signature enforcement or other Windows protections.

Reference documentation checked 2026-10-06:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/
- https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings
- https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-attestation
- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects

Provider choice and cost remain unresolved; do not purchase credentials until the
required signing route is verified for this APO and approved by the owner.

Account check: verify the Hardware program in Partner Center, publisher/legal
identity, enrollment eligibility and EV association requirements. Store/developer
account ownership alone is not evidence of hardware-submission access.

Current driver code-signing requirements:
https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-reqs
