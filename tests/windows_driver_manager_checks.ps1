# SPDX-License-Identifier: GPL-3.0-only
# Read-only package validation. Does not install, remove, or change security.
param(
    [Parameter(Mandatory=$true)][string]$Manager,
    [Parameter(Mandatory=$true)][string]$UnsignedPackage
)
$ErrorActionPreference = 'Stop'
$Manager = (Resolve-Path -LiteralPath $Manager).Path
$UnsignedPackage = (Resolve-Path -LiteralPath $UnsignedPackage).Path
foreach ($file in @('soundcurrentvad.inf','soundcurrentvad.sys','soundcurrentvad.cat')) {
    if (!(Test-Path -LiteralPath (Join-Path $UnsignedPackage $file))) {
        throw "Unsigned test package must be complete: $file is missing"
    }
}
& $Manager --status
if ($LASTEXITCODE -notin @(0,10)) { throw 'Driver status check failed' }
& $Manager --verify $UnsignedPackage
if ($LASTEXITCODE -ne 30) { throw 'Unsigned catalog was not rejected' }
& $Manager --verify (Join-Path $UnsignedPackage 'nonexistent')
if ($LASTEXITCODE -ne 30) { throw 'Incomplete package was not rejected' }
& $Manager --status unexpected
if ($LASTEXITCODE -ne 30) { throw 'Malformed command was not rejected' }
& $Manager --remove eq invalid-sid
if ($LASTEXITCODE -ne 30) { throw 'Malformed original-user SID was not rejected' }
$temporary=Join-Path $env:TEMP ('sc-package-identity-'+[guid]::NewGuid())
New-Item -ItemType Directory $temporary | Out-Null
try {
 $fixture=Join-Path $temporary 'package';New-Item -ItemType Directory $fixture | Out-Null
 foreach($file in @('soundcurrentvad.inf','soundcurrentvad.sys','soundcurrentvad.cat')) {
  Copy-Item (Join-Path $UnsignedPackage $file) $fixture
 }
 $inf=Join-Path $fixture 'soundcurrentvad.inf';$original=Get-Content -LiteralPath $inf -Raw
 function VerifyReject([string]$Expected) {
  $stdout=Join-Path $temporary 'out.txt';$stderr=Join-Path $temporary 'err.txt'
  $process=Start-Process -FilePath $Manager -ArgumentList @('--verify',('"{0}"' -f $fixture)) -PassThru -Wait -RedirectStandardOutput $stdout -RedirectStandardError $stderr
  $process.Refresh()
  if($process.ExitCode -ne 30 -or (Get-Content $stderr -Raw) -notmatch [regex]::Escape($Expected)) {throw "Wrong package validation gate: $Expected"}
 }
 VerifyReject 'Driver catalog is unsigned'
 foreach($changed in @(
  $original.Replace('Root\SOUNDCURRENTVAD','Root\OTHERDEVICE'),
  $original.Replace('CatalogFile=soundcurrentvad.cat','CatalogFile=other.cat'),
  $original.Replace('AddService=SoundCurrentVAD','AddService=OtherService'),
  $original.Replace('%DeviceName%=SoundCurrent,Root\SOUNDCURRENTVAD',"%DeviceName%=SoundCurrent,Root\SOUNDCURRENTVAD`r`n%DeviceName%=SoundCurrent,Root\OTHERDEVICE")
 )) {
  if($changed -eq $original){throw 'Identity fixture did not change'}
  Set-Content -LiteralPath $inf -Value $changed -Encoding Unicode
  VerifyReject 'Driver package identity does not match'
 }
 Write-Output 'PASS: valid identity reaches signature gate; wrong hardware/catalog/service and extra model rejected before trust/import'
} finally {Remove-Item -LiteralPath $temporary -Recurse -Force}
Write-Output 'PASS: status and unsigned/incomplete/malformed/SID rejection; no driver mutations'
