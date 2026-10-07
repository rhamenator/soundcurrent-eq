# SPDX-License-Identifier: GPL-3.0-only
# Use only in a full independent Windows VM clone. Temporarily sets a boot marker and creates an inert named process fixture.
param([switch]$Run)
if (!$Run) { Write-Output "Use -Run only in an independent Windows test clone."; exit 0 }
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function RunCode([string]$Exe,[string]$Arguments,[int]$Expected=0) {
 $info=New-Object System.Diagnostics.ProcessStartInfo
 $info.FileName=$Exe;$info.Arguments=$Arguments;$info.UseShellExecute=$false
 $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $p=New-Object System.Diagnostics.Process;$p.StartInfo=$info
 try {
  if(!$p.Start()){throw "Cannot start $Exe"}
  $out=$p.StandardOutput.ReadToEndAsync();$err=$p.StandardError.ReadToEndAsync()
  if(!$p.WaitForExit(45000)){$p.Kill();throw 'Regression timed out'}
  $text=$out.Result+$err.Result
  if($null -eq $p.ExitCode -or $p.ExitCode -ne $Expected){throw "Exit $($p.ExitCode), expected $Expected`: $text"}
  Write-Output $text
 }finally{$p.Dispose()}
}
$key='HKCU:\Software\SoundCurrent\VBCable'
$before=(Get-ItemProperty -LiteralPath $key -Name InstalledDuringBoot -ErrorAction SilentlyContinue).InstalledDuringBoot
$work=Join-Path $env:TEMP ('SoundCurrent-Reboot-Test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $work|Out-Null
try {
 foreach($repo in @('soundcurrent-eq','soundcurrent-studio')) {
  $root=Join-Path "$env:USERPROFILE\SoundCurrent-integrated-build" $repo
  $stage=Join-Path $work $repo
  New-Item -ItemType Directory $stage|Out-Null
  Copy-Item "$root\package\*" $stage -Recurse
  Copy-Item "$root\packaging\windows\cable-setup.ps1" "$stage\cable-setup.ps1"
  Copy-Item "$root\packaging\windows\cable-setup.ps1" "$stage\audio-setup.ps1"
  Copy-Item "$env:USERPROFILE\sc-cable.zip" "$stage\VBCABLE_Driver_Pack45.zip"
  $tokens=$null;$errors=$null
  [void][Management.Automation.Language.Parser]::ParseFile("$stage\cable-setup.ps1",[ref]$tokens,[ref]$errors)
  if($errors.Count){throw ($errors|Out-String)}
  $env:QT_QPA_PLATFORM='offscreen';$env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts'
  New-Item -Path $key -Force|Out-Null
  $stamp=(Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().Ticks.ToString()
  Set-ItemProperty -LiteralPath $key -Name InstalledDuringBoot -Value $stamp
  $shell="$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
  RunCode $shell ('-NoProfile -File "'+$stage+'\audio-setup.ps1" -Check') 3010
  foreach($action in @('install','settings')) {
   RunCode "$stage\$repo.exe" "--windows-audio-setup-test $action Restart"
  }
  if(Get-Process VBCABLE_ControlPanel -ErrorAction SilentlyContinue){throw 'Pending reboot opened the vendor control panel'}
  # The first check after a different boot clears our own stale marker.
  Set-ItemProperty -LiteralPath $key -Name InstalledDuringBoot -Value 'previous-boot-fixture'
  RunCode $shell ('-NoProfile -File "'+$stage+'\audio-setup.ps1" -Check') 0
  if((Get-ItemProperty -LiteralPath $key -Name InstalledDuringBoot -ErrorAction SilentlyContinue).InstalledDuringBoot){throw 'Stale reboot marker not cleared'}
  RunCode "$stage\$repo.exe" '--windows-audio-setup-test install "already installed"'
  [IO.File]::WriteAllText("$stage\VBCABLE_Driver_Pack45.zip",'corrupt package fixture')
  RunCode "$stage\$repo.exe" '--windows-audio-setup-test settings "checksum mismatch"'
  Rename-Item "$stage\audio-setup.ps1" 'temporarily-missing-setup.ps1'
  RunCode "$stage\$repo.exe" '--windows-audio-setup-test install "setup is missing"'
  # Real process enumeration must name only the running equalizer.
  $fixture=Join-Path $work 'soundcurrent-eq.exe'
  Copy-Item "$env:SystemRoot\System32\cmd.exe" $fixture -Force
  $info=New-Object System.Diagnostics.ProcessStartInfo
  $info.FileName=$fixture;$info.Arguments='/D /Q /K';$info.UseShellExecute=$false
  $info.RedirectStandardInput=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
  $client=New-Object System.Diagnostics.Process;$client.StartInfo=$info
  try {
   if(!$client.Start()){throw 'Cannot start named process fixture'}
   $drainOut=$client.StandardOutput.ReadToEndAsync();$drainErr=$client.StandardError.ReadToEndAsync()
   $message=RunCode $shell ('-NoProfile -File "'+$stage+'\cable-setup.ps1" -Install -Quiet') 30
   if(($message -join ' ') -notmatch 'Quit SoundCurrent EQ before' -or ($message -join ' ') -match 'SoundCurrent Studio') {throw "Incorrect running-app message: $message"}
  }finally{
   if(!$client.HasExited){$client.Kill();$client.WaitForExit()}
   $client.Dispose()
  }
  Write-Output "PASS: $repo pending reboot, restart clearing, setup keeps UI alive, visible package and missing-helper errors"
 }
} finally {
 if($before){Set-ItemProperty -LiteralPath $key -Name InstalledDuringBoot -Value $before}
 else{Remove-ItemProperty -LiteralPath $key -Name InstalledDuringBoot -ErrorAction SilentlyContinue}
 Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
'REBOOT REGRESSION COMPLETED'
