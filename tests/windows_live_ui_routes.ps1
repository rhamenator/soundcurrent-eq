# SPDX-License-Identifier: GPL-3.0-only
# Opt-in live endpoint/default-volume tests: independent Windows clone only.
param([switch]$Run,[string]$BuildRoot,[switch]$TwoPhysicalOutputs)
if(!$Run){Write-Output 'Use -Run only on an independent Windows test clone.';exit 0}
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
foreach($name in @('soundcurrent-eq','soundcurrent-studio')) {
 $root=Join-Path $base $name
 $env:PATH="$root\package;"+$env:PATH
 $env:QT_QPA_PLATFORM='offscreen'
 $env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts'
 $modes=@('--windows-live-ui-test','--windows-live-manual-route-test')
 if($TwoPhysicalOutputs){$modes+='--windows-live-manual-route-test --other-physical-output'}
 foreach($mode in $modes) {
  $info=New-Object System.Diagnostics.ProcessStartInfo
  $info.FileName="$root\package\$name.exe";$info.Arguments=$mode
  $info.UseShellExecute=$false;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
  $process=New-Object System.Diagnostics.Process;$process.StartInfo=$info
  if(!$process.Start()){throw 'Native launch failed'}
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
  if(!$process.WaitForExit(60000)){$process.Kill();throw 'Live UI test timed out'}
  $output=$stdout.Result;Write-Output $output;Write-Output $stderr.Result
  $code=$process.ExitCode
  if($null -eq $code -or $code -ne 0){throw "$name $mode failed: $code"}
  if($mode.StartsWith('--windows-live-manual-route-test') -and $output -notmatch 'PASS: direct Windows output choice'){throw 'Missing explicit completion marker'}
  Write-Output "$name $mode VERIFIED EXIT $code"
  $process.Dispose()
 }
}
