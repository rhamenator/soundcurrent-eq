# SPDX-License-Identifier: GPL-3.0-only
# Independent Windows clone only; starts/kills its own guard test process.
param([switch]$Run,[string]$BuildRoot)
if(!$Run){Write-Output 'Use -Run only on an independent Windows test clone.';exit 0}
$ErrorActionPreference='Stop'
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
foreach($name in @('soundcurrent-eq','soundcurrent-studio')) {
 $root=Join-Path $base $name
 $env:PATH="$root\package;"+$env:PATH
 $test="$root\build-windows-integrated\Release\soundcurrent-processing-guard-test.exe"
 & $test
 if($LASTEXITCODE){throw 'Processing guard test failed'}
 $directory=Join-Path $env:TEMP ('sc-gate-'+[guid]::NewGuid())
 New-Item -ItemType Directory $directory | Out-Null
 $out=Join-Path $directory 'ready.txt'
 $owner=Start-Process -FilePath $test -ArgumentList @('--hold',$directory) -PassThru -RedirectStandardOutput $out
 try {
  $ready=$false
  for($i=0;$i -lt 100;$i++){if((Get-Content $out -Raw -ErrorAction SilentlyContinue) -match 'ready'){$ready=$true;break};Start-Sleep -Milliseconds 50}
  if(!$ready){throw 'Guard holder did not start'}
  $sid=[System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
  & "$root\build-windows-integrated\native\windows\driver-manager\Release\soundcurrent-driver-manager.exe" --remove eq $sid
  if($LASTEXITCODE -ne 30){throw 'Driver setup was not refused while app gate held'}
 } finally {if(!$owner.HasExited){$owner.Kill();$owner.WaitForExit()};Remove-Item $directory -Recurse}
 Write-Output "$name SESSION GATE TESTS PASSED"
}
