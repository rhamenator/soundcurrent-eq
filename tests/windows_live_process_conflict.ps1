# SPDX-License-Identifier: GPL-3.0-only
# Actual Toolhelp discovery with harmless renamed Windows cmd fixtures.
# This does not install or execute third-party equalizer software.
param([switch]$Run,[string]$BuildRoot)
if(!$Run){Write-Output 'Use -Run only on an independent Windows test clone.';exit 0}
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
function CheckConflict([string]$Executable,[string]$Expected) {
 $info=New-Object System.Diagnostics.ProcessStartInfo
 $info.FileName=$Executable
 $info.Arguments=if($Expected){'--expect-process-conflict '+$Expected}else{'--expect-no-process-conflict'}
 $info.UseShellExecute=$false;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $p=New-Object System.Diagnostics.Process;$p.StartInfo=$info
 try {
  if(!$p.Start()){throw 'Conflict probe launch failed'}
  $stdout=$p.StandardOutput.ReadToEndAsync();$stderr=$p.StandardError.ReadToEndAsync()
  if(!$p.WaitForExit(15000)){$p.Kill();throw 'Conflict probe timed out'}
  $output=$stdout.Result;Write-Output $output;Write-Output $stderr.Result
  $code=$p.ExitCode
  if($null -eq $code -or $code -ne 0 -or $output -notmatch 'PASS:'){throw "Conflict probe failed: $code"}
 }finally{$p.Dispose()}
}
$fixture=Join-Path $env:TEMP ('SoundCurrent-conflict-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
 foreach($identity in @('FxSound.exe','Peace.exe','SoundCurrent-unrelated.exe')) {
  $file=Join-Path $fixture $identity
  Copy-Item (Join-Path $env:SystemRoot 'System32\cmd.exe') $file
  $info=New-Object System.Diagnostics.ProcessStartInfo
  $info.FileName=$file;$info.Arguments='/D /Q /K'
  $info.UseShellExecute=$false;$info.RedirectStandardInput=$true
  $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
  $owner=New-Object System.Diagnostics.Process;$owner.StartInfo=$info
  try {
   if(!$owner.Start()){throw 'Fixture launch failed'}
   $stdout=$owner.StandardOutput.ReadToEndAsync();$stderr=$owner.StandardError.ReadToEndAsync()
   if($owner.WaitForExit(250)){throw 'Fixture exited before discovery'}
   foreach($name in @('soundcurrent-eq','soundcurrent-studio')) {
    $root=Join-Path $base $name
    $env:PATH="$root\package;"+$env:PATH
    $expected=if($identity -eq 'SoundCurrent-unrelated.exe'){''}else{$identity}
    CheckConflict "$root\package\soundcurrent-processing-guard-test.exe" $expected
   }
  }finally{
   if(!$owner.HasExited){$owner.Kill();$owner.WaitForExit()}
   $owner.Dispose()
  }
  Remove-Item -LiteralPath $file
 }
 foreach($name in @('soundcurrent-eq','soundcurrent-studio')) {
  $root=Join-Path $base $name
  $env:PATH="$root\package;"+$env:PATH
  CheckConflict "$root\package\soundcurrent-processing-guard-test.exe" ''
 }
 Write-Output 'PASS: recognized live process fixtures, unrelated process and cleanup recovery'
}finally{Remove-Item -LiteralPath $fixture -Recurse -Force}
