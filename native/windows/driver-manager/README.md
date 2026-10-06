# Shared driver manager prototype

GPL-3.0-only userspace helper, built separately from the MS-PL kernel driver.
Commands: `--status`, `--verify PACKAGE`, `--install eq|studio PACKAGE`,
`--remove eq|studio`. Install/remove accept an optional final original-user SID
argument, validated and normalized before setup changes.

Install verifies a trusted catalog and its INF association after copying the
three exact package files into protected Program Files staging. Windows then
enforces kernel signature requirements through the ordinary PnP API. A failed
new-device installation rolls back the new root device. No Windows security
settings are altered. Existing physical hardware drivers are never selected.

Install/remove require administrator approval and refuse while either app or
its route guardian is running. Owners are tracked separately for each account
SID and app in the 64-bit HKLM registry. Removing one owner retains the device
while another owner remains. Unknown owner removal does nothing. A shared
machine-wide mutex serializes setup across both apps. Imported OEM INF names
are journaled in protected HKLM storage before binding the device. Last-owner
removal deletes the virtual device, verifies each journaled INF's hardware ID
and catalog name, then asks Windows to delete unused packages without forcing
removal. Failed cleanup retains ownership and journal entries for retry.

## Unfinished gates

This is not yet connected to NSIS or the in-app setup control. The native setup
wrapper resolves the original user's SID before UAC and passes it explicitly;
alternate-account elevation still needs an end-to-end test. Driver Store cleanup
and rollback/reboot behavior need live signed-package verification. Interrupted
updates and multiple users need lifecycle tests. Signed package acceptance and real installation remain untested. Do not treat compilation or
unsigned-package refusal as successful installation evidence.

## Verified so far

The helper compiles with MinGW and native MSVC C++20. Read-only checks on the
independent VM reported the SoundCurrent driver absent and rejected both an incomplete package
and a complete package with an unsigned catalog. No installation was attempted
by these checks. The registry owner lifecycle and PnP changes remain untested.

Recovery ownership is persisted before importing or binding a trusted
package. A failed/interrupted install intentionally keeps that app owner,
so a later uninstall can retry device/package cleanup. Package journal writes
are flushed before binding. There remains an import-to-journal interruption
window: discovering an imported but unjournaled OEM package requires further
recovery work. These changes have not been fault-injected with a signed
driver package.

Imports now use `SP_COPY_NOOVERWRITE` and accept `ERROR_FILE_EXISTS` only
when Windows returns the existing OEM INF name. This allows a retry with
the same signed INF/catalog to journal an already imported package. See
[SetupCopyOEMInfW](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupcopyoeminfw).
`SetupGetInfPublishedName` does not accept an arbitrary staged source INF as
a content search, so it is unsuitable for recovering this window. Recovery
without the original installer still requires durable import intent and
protected staged-package retention; that work remains pending.

Durable import recovery is now implemented: a flushed HKLM64 PendingImport
record names a protected Program Files staging directory before import.
If import or package journaling fails, staging is retained. Subsequent setup
or an owned uninstall re-verifies trust, retries NOOVERWRITE import, flushes
the OEM package journal, retires the intent, and deletes the retained stage.
Recovery rejects paths outside the exact staging parent, non-GUID directory
names, missing/redirected ancestors and malformed registry strings. Signed
interruption fault injection and orphan-stage cleanup after record retirement
remain unverified/incomplete. Earlier pending statements above are historical.

Staging retirement now uses a durable cleanup phase in the same import
record. That phase remains until the three known package files and staging
directory are removed. Recovery handles partial deletion and a directory
already deleted before interruption; unknown files are retained and stop
cleanup. `tests/windows_driver_cleanup_recovery.ps1 -Run` exercises synthetic
cleanup state on an independent elevated clone, refusing existing owners or
own driver. Both managers passed these cases with audio inventory unchanged.
It does not import a driver or bypass signature verification.
