# SPDX-License-Identifier: GPL-3.0-only
# Independent clone only; changes guest endpoint levels and restores originals.
param([switch]$Run,[string]$BuildRoot)
if(!$Run){Write-Output 'Use -Run only on an independent clone with the USB fixture.';exit 0}
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
$physical=@('{0.0.0.00000000}.{4773149C-749F-4680-AC80-653A70B000C5}','{0.0.0.00000000}.{3D2EA70D-86DC-498E-8B66-D27BDEB3A792}')
$owned='{0.0.0.00000000}.{0E71CA96-ED65-4271-8637-4C2920954B29}'
function ReadLevel($tool,$id){
 $lines=@(& $tool --read-volume $id)
 if($LASTEXITCODE -ne 0 -or $lines.Count -ne 1){throw 'Volume read failed'}
 $bits=$lines[0] -split ' '
 if($bits.Count -ne 2){throw 'Malformed level probe'}
 return @([double]::Parse($bits[0],[Globalization.CultureInfo]::InvariantCulture),[int]$bits[1])
}
function SetLevel($tool,$id,$level,$mute){
 & $tool --set-volume $id ([double]$level).ToString([Globalization.CultureInfo]::InvariantCulture) $mute
 if($LASTEXITCODE -ne 0){throw 'Fixture level change failed'}
}
foreach($name in @('soundcurrent-eq','soundcurrent-studio')) {
 $package=Join-Path $base "$name\package";$env:PATH="$package;"+$env:PATH
 $env:QT_QPA_PLATFORM='offscreen';$env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts'
 $tool=Join-Path $package 'soundcurrent-windows-volume-smoke.exe'
 $probe=Join-Path $package 'soundcurrent-windows-route-smoke.exe'
 $saved=@{};foreach($id in ($physical+@($owned))){$saved[$id]=ReadLevel $tool $id}
 try {
  foreach($extra in @('',' --other-physical-output')) {
   $before=@(& $probe --snapshot);if($LASTEXITCODE -ne 0 -or $before.Count -ne 3){throw 'Snapshot failed'}
   $choice=$before[0]
   if($physical -notcontains $choice -or $before[1] -ne $choice -or $before[2] -ne $choice){throw 'Need consistent physical defaults before test'}
   foreach($id in $physical){SetLevel $tool $id 0.71 1};SetLevel $tool $owned 0.35 1
   $info=New-Object System.Diagnostics.ProcessStartInfo
   $info.FileName=Join-Path $package "$name.exe";$info.Arguments='--windows-live-manual-route-test'+$extra
   $info.UseShellExecute=$false;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
   $app=New-Object System.Diagnostics.Process;$app.StartInfo=$info
   try {
    if(!$app.Start()){throw 'Desktop launch failed'}
    $stdout=$app.StandardOutput.ReadToEndAsync();$stderr=$app.StandardError.ReadToEndAsync()
    $deadline=[DateTime]::UtcNow.AddSeconds(8);$seenOwned=$false;$selected=$false
    do {
     $roles=@(& $probe --snapshot);if($LASTEXITCODE -ne 0){throw 'Route read failed'}
     if($roles.Count -eq 3 -and $roles[0] -eq $owned){$seenOwned=$true}
     if($seenOwned -and $roles.Count -eq 3 -and $roles[0] -eq $choice -and $roles[1] -eq $choice -and $roles[2] -eq $choice){$selected=$true;break}
     if($app.HasExited){Write-Output $stderr.Result;throw 'Desktop exited before direct selection'}
     Start-Sleep -Milliseconds 25
    }while([DateTime]::UtcNow -lt $deadline)
    if(!$selected){throw 'Direct physical selection not observed'}
    # The user adjusts this physical output after choosing it in Windows.
    SetLevel $tool $choice 0.58 0
    if(!$app.WaitForExit(8000)){throw 'Desktop did not finish'}
    $output=$stdout.Result;Write-Output $output;Write-Output $stderr.Result
    $code=$app.ExitCode
    if($null -eq $code -or $code -ne 0 -or $output -notmatch 'PASS: direct Windows output choice'){throw 'Desktop route check failed'}
    foreach($id in $physical){
     $actual=ReadLevel $tool $id
     $expected=if($id -eq $choice){@(0.58,0)}else{@(0.71,1)}
     if([math]::Abs($actual[0]-$expected[0]) -gt 0.005 -or $actual[1] -ne $expected[1]){throw "Manual physical level/mute overwritten: $id actual $actual expected $expected"}
    }
    Write-Output "PASS: $name$extra direct selection preserves adjusted physical level/mute and releases old output lease"
   }finally{
    if(!$app.HasExited){$app.Kill();if(!$app.WaitForExit(10000)){throw 'Desktop cleanup failed'}}
    $app.Dispose()
   }
  }
 }finally{foreach($id in $saved.Keys){SetLevel $tool $id $saved[$id][0] $saved[$id][1]}}
}
