# SPDX-License-Identifier: GPL-3.0-only
# Opt-in independent-clone test of ordinary desktop startup/conflict monitor.
param([switch]$Run,[string]$BuildRoot)
if(!$Run){Write-Output 'Use -Run only on an independent Windows test clone.';exit 0}
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
$fixture=Join-Path $env:TEMP ('SoundCurrent-window-conflict-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
 foreach($identity in @('FxSound.exe','Peace.exe')) {
  $file=Join-Path $fixture $identity
  Copy-Item (Join-Path $env:SystemRoot 'System32\cmd.exe') $file
  foreach($name in @('soundcurrent-eq','soundcurrent-studio')) {
   $package=Join-Path $base "$name\package"
   $env:PATH="$package;"+$env:PATH
   $env:QT_QPA_PLATFORM='offscreen';$env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts'
   $probe=Join-Path $package 'soundcurrent-windows-route-smoke.exe'
   $before=@(& $probe --snapshot)
   if($LASTEXITCODE -ne 0 -or $before.Count -ne 3){throw 'Cannot snapshot three output roles'}
   $phase=Join-Path $fixture "$name-phase.txt"
   Remove-Item -LiteralPath $phase -ErrorAction SilentlyContinue
   $info=New-Object System.Diagnostics.ProcessStartInfo
   $info.FileName=Join-Path $package "$name.exe"
   $info.Arguments='--windows-live-conflict-test "'+$phase+'"'
   $info.UseShellExecute=$false;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
   $app=New-Object System.Diagnostics.Process;$app.StartInfo=$info
   $owner=$null
   try {
    if(!$app.Start()){throw 'Desktop launch failed'}
    $stdout=$app.StandardOutput.ReadToEndAsync();$stderr=$app.StandardError.ReadToEndAsync()
    $deadline=[DateTime]::UtcNow.AddSeconds(15);$ready=$false
    do {
     if(Test-Path -LiteralPath $phase){if((Get-Content -LiteralPath $phase -Raw).Trim() -eq 'ready'){$ready=$true;break}}
     if($app.HasExited){Write-Output $stderr.Result;throw 'Desktop exited before processing'}
     Start-Sleep -Milliseconds 100
    }while([DateTime]::UtcNow -lt $deadline)
    if(!$ready){throw 'Desktop did not become ready'}
    $guardians=@(Get-CimInstance Win32_Process -Filter "Name='soundcurrent-route-guardian.exe'" | Where-Object ParentProcessId -eq $app.Id)
    if($guardians.Count -ne 1){throw 'Desktop route guardian missing'}
    & $probe --verify-defaults $before[0] $before[1] $before[2]
    if($LASTEXITCODE -eq 0){throw 'App did not change output roles'}
    $info=New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName=$file;$info.Arguments='/D /Q /K'
    $info.UseShellExecute=$false;$info.RedirectStandardInput=$true
    $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $owner=New-Object System.Diagnostics.Process;$owner.StartInfo=$info
    if(!$owner.Start()){throw 'Fixture launch failed'}
    $fixtureOut=$owner.StandardOutput.ReadToEndAsync();$fixtureErr=$owner.StandardError.ReadToEndAsync()
    if($owner.WaitForExit(250)){throw 'Conflict fixture exited early'}
    if(!$app.WaitForExit(12000)){throw 'Desktop failed to stop for new conflict'}
    $output=$stdout.Result;$errors=$stderr.Result
    Write-Output $output;Write-Output $errors
    $code=$app.ExitCode
    if($null -eq $code -or $code -ne 0 -or $errors -notmatch ('Live conflict: '+[regex]::Escape($identity)) -or $errors -notmatch 'PASS: processing off and route released before conflict dialog'){throw "Desktop conflict exit/diagnostic failed: $code"}
    & $probe --verify-defaults $before[0] $before[1] $before[2]
    if($LASTEXITCODE -ne 0){throw 'Conflict shutdown did not restore original output roles'}
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    do {
     $left=@(Get-Process -Id $guardians[0].ProcessId -ErrorAction SilentlyContinue)
     if(!$left.Count){break};Start-Sleep -Milliseconds 100
    }while([DateTime]::UtcNow -lt $deadline)
    if($left.Count){throw 'Own guardian remained after conflict shutdown'}
    Write-Output "PASS: $name normal monitor stopped for $identity; three roles restored; guardian exited"
   }finally{
    if($owner){if(!$owner.HasExited){$owner.Kill();if(!$owner.WaitForExit(5000)){throw 'Fixture cleanup failed'}};$owner.Dispose()}
    if(!$app.HasExited){$app.Kill();if(!$app.WaitForExit(10000)){throw 'Desktop cleanup failed'}}
    $app.Dispose()
   }
  }
  Remove-Item -LiteralPath $file
 }
}finally{Remove-Item -LiteralPath $fixture -Recurse -Force}
