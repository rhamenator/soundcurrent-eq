# SPDX-License-Identifier: GPL-3.0-only
param([switch]$Run,[string]$BuildRoot)
if(!$Run){Write-Output 'Use -Run only on an independent Windows clone with the two-output USB fixture.';exit 0}
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
foreach($name in @('soundcurrent-eq','soundcurrent-studio')) {
 $root=Join-Path $base $name;$package=Join-Path $root 'package'
 $env:PATH="$package;"+$env:PATH
 $env:QT_QPA_PLATFORM='offscreen';$env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts'
 $probe=Join-Path $package 'soundcurrent-windows-route-smoke.exe'
 $before=@(& $probe --snapshot)
 if($LASTEXITCODE -ne 0 -or $before.Count -ne 3){throw 'Cannot snapshot all three defaults'}
 $phase=Join-Path $env:TEMP "$name-whole-app-crash.txt"
 Remove-Item -LiteralPath $phase -ErrorAction SilentlyContinue
 $info=New-Object System.Diagnostics.ProcessStartInfo
 $info.FileName=Join-Path $package "$name.exe"
 $info.Arguments='--windows-live-hotplug-test "'+$phase+'"'
 $info.UseShellExecute=$false;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $app=New-Object System.Diagnostics.Process;$app.StartInfo=$info
 try {
  if(!$app.Start()){throw 'Desktop start failed'}
  $stdout=$app.StandardOutput.ReadToEndAsync();$stderr=$app.StandardError.ReadToEndAsync()
  $deadline=[DateTime]::UtcNow.AddSeconds(20);$ready=$false
  do {
   if(Test-Path $phase){if((Get-Content $phase -Raw).Trim() -eq 'ready'){$ready=$true;break}}
   if($app.HasExited){throw 'Desktop exited before acquiring route'}
   Start-Sleep -Milliseconds 100
  }while([DateTime]::UtcNow -lt $deadline)
  if(!$ready){throw 'Desktop route not ready'}
  $guardians=@(Get-CimInstance Win32_Process -Filter "Name='soundcurrent-route-guardian.exe'" | Where-Object ParentProcessId -eq $app.Id)
  if($guardians.Count -ne 1){throw 'Actual desktop guardian not found'}
  # Confirm that this live app actually changed the captured defaults.
  & $probe --verify-defaults $before[0] $before[1] $before[2]
  if($LASTEXITCODE -eq 0){throw 'Desktop did not acquire a changed route'}
  $app.Kill();if(!$app.WaitForExit(10000)){throw 'Desktop did not terminate'}
  $exitCode=$app.ExitCode
  if($null -eq $exitCode -or $exitCode -eq 0){throw 'Expected forced desktop termination'}
  $deadline=[DateTime]::UtcNow.AddSeconds(12);$restored=$false
  do {
   & $probe --verify-defaults $before[0] $before[1] $before[2]
   if($LASTEXITCODE -eq 0){$restored=$true;break}
   Start-Sleep -Milliseconds 100
  }while([DateTime]::UtcNow -lt $deadline)
  if(!$restored){throw 'Desktop crash did not restore original output roles'}
  $deadline=[DateTime]::UtcNow.AddSeconds(5)
  do {
   $left=@(Get-Process -Id $guardians[0].ProcessId -ErrorAction SilentlyContinue)
   if(!$left.Count){break};Start-Sleep -Milliseconds 100
  }while([DateTime]::UtcNow -lt $deadline)
  if($left.Count){throw 'Desktop recovery guardian remained alive'}
  Write-Output "PASS: $name whole app terminated ($exitCode); original three roles restored; own guardian exited"
 } finally {
  if(!$app.HasExited){$app.Kill();$app.WaitForExit()}
  $app.Dispose();Remove-Item -LiteralPath $phase -ErrorAction SilentlyContinue
 }
}
