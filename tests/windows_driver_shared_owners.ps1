# SPDX-License-Identifier: GPL-3.0-only
# Synthetic registry ownership tests: elevated independent clone only.
param([switch]$Run,[string]$BuildRoot)
if(!$Run){Write-Output "Use -Run only on an independent Windows test clone.";exit 0}
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
$eq=Join-Path $base 'soundcurrent-eq\build-windows-integrated\native\windows\driver-manager\Release\soundcurrent-driver-manager.exe'
$studio=Join-Path $base 'soundcurrent-studio\build-windows-integrated\native\windows\driver-manager\Release\soundcurrent-driver-manager.exe'
$reg=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::Registry64)
$key=$reg.CreateSubKey('Software\SoundCurrent\AudioDriver')
$owners=$key.CreateSubKey('Owners')
$packages=$key.CreateSubKey('Packages')
if($owners.ValueCount -or $packages.ValueCount -or $key.GetValue('PendingImport')) {throw 'Existing shared driver state: preserve it'}
& $eq --status
if($LASTEXITCODE -ne 10) {throw 'Own driver exists: preserve it'}
$sid=[System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
# A second valid SID is only synthetic ownership metadata, not a login/account.
$secondSid='S-1-5-21-123456789-234567890-345678901-9999'
$eqOwner="$sid.eq";$studioOwner="$secondSid.studio"
$before=Get-CimInstance Win32_PnPEntity | Where-Object PNPClass -eq 'MEDIA' | Select-Object PNPDeviceID,Service,Status,ConfigManagerErrorCode | Sort-Object PNPDeviceID | ConvertTo-Json -Compress
$mutex=$null;$held=$false
# A missing INF models interruption after Driver Store deletion but before
# journal retirement. Refuse to select any filename that actually exists.
$missingPackage='oem2147483647.inf'
if(Test-Path (Join-Path $env:windir ('INF\'+$missingPackage))) {throw 'Synthetic journal name exists: preserve it'}
try {
 $owners.SetValue($eqOwner,1,[Microsoft.Win32.RegistryValueKind]::DWord)
 $owners.SetValue($studioOwner,1,[Microsoft.Win32.RegistryValueKind]::DWord);$owners.Flush()
 & $eq --remove eq $sid
 if($LASTEXITCODE -ne 0 -or $owners.GetValue($eqOwner) -or !$owners.GetValue($studioOwner)) {throw 'First-owner removal affected another user/app'}
 & $eq --remove eq $sid
 if($LASTEXITCODE -ne 0 -or !$owners.GetValue($studioOwner)) {throw 'Repeated unowned removal affected another app'}
 $mutex=New-Object System.Threading.Mutex($false,'Global\SoundCurrent.AudioDriver.Transaction.v1')
 $held=$mutex.WaitOne(1000)
 if(!$held){throw 'Cannot acquire test transaction mutex'}
 $timer=[System.Diagnostics.Stopwatch]::StartNew()
 & $studio --remove studio $secondSid
 $timer.Stop()
 if($LASTEXITCODE -ne 30 -or !$owners.GetValue($studioOwner) -or $timer.Elapsed.TotalSeconds -lt 14 -or $timer.Elapsed.TotalSeconds -gt 25) {throw 'Concurrent setup did not time out without mutation'}
 $mutex.ReleaseMutex();$held=$false
 $packages.SetValue($missingPackage,1,[Microsoft.Win32.RegistryValueKind]::DWord);$packages.Flush()
 & $studio --remove studio $secondSid
 if($LASTEXITCODE -ne 0 -or $owners.ValueCount -or $packages.ValueCount) {throw 'Last owner removal failed'}
 Write-Output 'PASS: shared ownership across apps/SIDs, repeated removal, bounded transaction lock, missing-package journal retirement'
} finally {
 if($held){$mutex.ReleaseMutex()}
 if($mutex){$mutex.Dispose()}
 $owners.DeleteValue($eqOwner,$false);$owners.DeleteValue($studioOwner,$false)
 $packages.DeleteValue($missingPackage,$false)
 $packages.Dispose();$owners.Dispose();$key.Dispose();$reg.Dispose()
}
$after=Get-CimInstance Win32_PnPEntity | Where-Object PNPClass -eq 'MEDIA' | Select-Object PNPDeviceID,Service,Status,ConfigManagerErrorCode | Sort-Object PNPDeviceID | ConvertTo-Json -Compress
if($before -ne $after){throw 'Audio device inventory changed'}
Write-Output 'PASS: physical/third-party audio inventory unchanged; no driver imported'
