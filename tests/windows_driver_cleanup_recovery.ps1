# SPDX-License-Identifier: GPL-3.0-only
# Independent clone only: writes synthetic HKLM ownership/recovery records.
# Requires an elevated shell; refuses existing driver/ownership/import state.
param([switch]$Run,[string]$BuildRoot)
if(!$Run){Write-Output "Use -Run only on an independent Windows test clone.";exit 0}
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
$reg=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::Registry64)
$keyPath='Software\SoundCurrent\AudioDriver'
$check=$reg.OpenSubKey($keyPath)
if($check) {
 if($check.GetValue('PendingImport')) { throw 'Existing import intent: preserve it' }
 $ownersCheck=$check.OpenSubKey('Owners')
 if($ownersCheck -and $ownersCheck.ValueCount) { throw 'Existing owners: preserve them' }
 if($ownersCheck) {$ownersCheck.Dispose()}
 $check.Dispose()
}
$sid=[System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$before=Get-CimInstance Win32_PnPEntity | Where-Object PNPClass -eq 'MEDIA' | Select-Object PNPDeviceID,Service,Status,ConfigManagerErrorCode | Sort-Object PNPDeviceID | ConvertTo-Json -Compress
foreach($name in @('soundcurrent-eq','soundcurrent-studio')) {
 $app=if($name -eq 'soundcurrent-eq') {'eq'} else {'studio'}
 $manager=Join-Path $base "$name\build-windows-integrated\native\windows\driver-manager\Release\soundcurrent-driver-manager.exe"
 & $manager --status
 if($LASTEXITCODE -ne 10) {throw 'Own driver exists: preserve it'}
 $stage=Join-Path ([Environment]::GetFolderPath('ProgramFiles')) ('SoundCurrent\AudioDriver\{'+[guid]::NewGuid().ToString().ToUpper()+'}')
 $key=$reg.CreateSubKey($keyPath)
 $owners=$key.CreateSubKey('Owners')
 $owner="$sid.$app"
 try {
  New-Item -ItemType Directory -Force $stage | Out-Null
  foreach($file in @('soundcurrentvad.inf','soundcurrentvad.sys','soundcurrentvad.cat')) { Set-Content -LiteralPath (Join-Path $stage $file) -Value 'synthetic cleanup fixture only' }
  Set-Content -LiteralPath (Join-Path $stage 'unknown.txt') -Value 'preserve'
  $owners.SetValue($owner,1,[Microsoft.Win32.RegistryValueKind]::DWord); $owners.Flush()
  $key.SetValue('PendingImport',"cleanup|$stage",[Microsoft.Win32.RegistryValueKind]::String);$key.Flush()
  & $manager --remove $app $sid
  if($LASTEXITCODE -ne 30) {throw 'Unknown staging file was not preserved'}
  if(!(Test-Path (Join-Path $stage 'unknown.txt')) -or !$key.GetValue('PendingImport') -or !$owners.GetValue($owner)) {throw 'Cleanup failure lost file, record or ownership'}
  Remove-Item -LiteralPath (Join-Path $stage 'unknown.txt')
  & $manager --remove $app $sid
  if($LASTEXITCODE -ne 0 -or (Test-Path $stage) -or $key.GetValue('PendingImport') -or $owners.GetValue($owner)) {throw 'Partial cleanup retry failed'}
  # Crash after directory deletion but before retiring its record.
  $owners.SetValue($owner,1,[Microsoft.Win32.RegistryValueKind]::DWord);$owners.Flush()
  $key.SetValue('PendingImport',"cleanup|$stage",[Microsoft.Win32.RegistryValueKind]::String);$key.Flush()
  & $manager --remove $app $sid
  if($LASTEXITCODE -ne 0 -or $key.GetValue('PendingImport') -or $owners.GetValue($owner)) {throw 'Missing-stage cleanup retry failed'}
  Write-Output "$name CLEANUP RECOVERY PASSED"
 } finally {
  $key.DeleteValue('PendingImport',$false);$owners.DeleteValue($owner,$false)
  $owners.Dispose();$key.Dispose()
  if(Test-Path $stage) {Remove-Item -LiteralPath $stage -Recurse}
 }
}
$after=Get-CimInstance Win32_PnPEntity | Where-Object PNPClass -eq 'MEDIA' | Select-Object PNPDeviceID,Service,Status,ConfigManagerErrorCode | Sort-Object PNPDeviceID | ConvertTo-Json -Compress
if($before -ne $after) {throw 'Audio device inventory changed'}
$reg.Dispose()
Write-Output 'PASS: partial cleanup, unknown-file preservation, missing-stage retry; audio inventory unchanged'
