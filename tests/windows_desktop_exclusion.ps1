# SPDX-License-Identifier: GPL-3.0-only
param([switch]$Run,[string]$BuildRoot)
if(!$Run){Write-Output 'Use -Run only on an independent Windows clone with the USB fixture.';exit 0}
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
function StartNative([string]$File,[string]$Arguments) {
 $info=New-Object System.Diagnostics.ProcessStartInfo
 $info.FileName=$File;$info.Arguments=$Arguments;$info.UseShellExecute=$false
 $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $p=New-Object System.Diagnostics.Process;$p.StartInfo=$info
 if(!$p.Start()){throw 'Native launch failed'}
 return @{process=$p;stdout=$p.StandardOutput.ReadToEndAsync();stderr=$p.StandardError.ReadToEndAsync()}
}
foreach($ownerName in @('soundcurrent-eq','soundcurrent-studio')) {
 $otherName=if($ownerName -eq 'soundcurrent-eq'){'soundcurrent-studio'}else{'soundcurrent-eq'}
 $ownerRoot=Join-Path $base "$ownerName\package";$otherRoot=Join-Path $base "$otherName\package"
 $env:PATH="$ownerRoot;$otherRoot;"+$env:PATH
 $env:QT_QPA_PLATFORM='offscreen';$env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts'
 $phase=Join-Path $env:TEMP "$ownerName-exclusion-ready.txt"
 Remove-Item $phase -ErrorAction SilentlyContinue
 $owner=StartNative "$ownerRoot\$ownerName.exe" ('--windows-live-hotplug-test "'+$phase+'"')
 try {
  $deadline=[DateTime]::UtcNow.AddSeconds(20);$ready=$false
  do {
   if(Test-Path $phase){if((Get-Content $phase -Raw).Trim() -eq 'ready'){$ready=$true;break}}
   if($owner.process.HasExited){throw 'Owner exited before acquiring route'}
   Start-Sleep -Milliseconds 100
  }while([DateTime]::UtcNow -lt $deadline)
  if(!$ready){throw 'Owner not ready'}
  $other=StartNative "$otherRoot\$otherName.exe" '--windows-live-manual-route-test'
  try {
   if(!$other.process.WaitForExit(15000)){$other.process.Kill();throw 'Contender timed out'}
   $code=$other.process.ExitCode;$message=$other.stderr.Result
   if($null -eq $code -or $code -ne 1 -or $message -notmatch 'Audio session busy'){throw "Wrong conflict result: $code / $message"}
   Write-Output "PASS: $ownerName owns live audio; $otherName rejected specifically for session conflict"
  } finally {$other.process.Dispose()}
  $owner.process.Kill();$owner.process.WaitForExit()
  $deadline=[DateTime]::UtcNow.AddSeconds(10)
  do {
   $children=@(Get-CimInstance Win32_Process -Filter "Name='soundcurrent-route-guardian.exe'" | Where-Object ParentProcessId -eq $owner.process.Id)
   if(!$children.Count){break};Start-Sleep -Milliseconds 100
  }while([DateTime]::UtcNow -lt $deadline)
  if($children.Count){throw 'Owner guardian did not exit'}
  $after=StartNative "$otherRoot\$otherName.exe" '--windows-live-manual-route-test'
  try {
   if(!$after.process.WaitForExit(15000)){$after.process.Kill();throw 'Post-owner start timed out'}
   $code=$after.process.ExitCode;$output=$after.stdout.Result
   if($null -eq $code -or $code -ne 0 -or $output -notmatch 'PASS: direct Windows output choice'){throw "Post-owner start failed: $code / $($after.stderr.Result)"}
   Write-Output "PASS: $otherName acquired audio after $ownerName terminated"
  } finally {$after.process.Dispose()}
 } finally {
  if(!$owner.process.HasExited){$owner.process.Kill();$owner.process.WaitForExit()}
  $owner.process.Dispose();Remove-Item $phase -ErrorAction SilentlyContinue
 }
}
