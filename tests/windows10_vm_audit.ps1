# SPDX-License-Identifier: GPL-3.0-only
# Disposable Windows 10 test only; no driver installation/trust/test-mode changes.
param([switch]$DisposableVM,[string]$OutputDirectory='C:\SoundCurrentAudit10',
 [string]$MediaRoot=$PSScriptRoot,
 [ValidatePattern('^[A-Fa-f0-9]{7,40}$')][string]$EqRevision='874f5ab',
 [ValidatePattern('^[A-Fa-f0-9]{7,40}$')][string]$StudioRevision='9037029')
$ErrorActionPreference='Stop'
if(!$DisposableVM -or (Get-CimInstance Win32_ComputerSystem).Manufacturer -notmatch '^QEMU$'){throw 'Explicit disposable QEMU VM opt-in required'}
foreach($name in 'SoundCurrent EQ','SoundCurrent Studio'){
 if(Test-Path (Join-Path $env:LOCALAPPDATA "Programs\$name")){throw 'Use a fresh VM without existing SoundCurrent installs'}
}
if(Test-Path $OutputDirectory){throw 'Choose a fresh output directory'}
$started=Get-Date
$root=$OutputDirectory
New-Item -ItemType Directory -Force $root|Out-Null
$results=New-Object System.Collections.Generic.List[object]
$failure=$null
function Run($exe,$arguments,$name){
 $out=Join-Path $root ($name+'.out.txt');$err=Join-Path $root ($name+'.err.txt')
 $info=New-Object System.Diagnostics.ProcessStartInfo
 $info.FileName=$exe;$info.Arguments=$arguments;$info.UseShellExecute=$false
 $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $info.StandardOutputEncoding=[Text.Encoding]::UTF8;$info.StandardErrorEncoding=[Text.Encoding]::UTF8
 $p=New-Object System.Diagnostics.Process;$p.StartInfo=$info
 try{
  if(!$p.Start()){throw "Could not start: $name"}
  $stdout=$p.StandardOutput.ReadToEndAsync();$stderr=$p.StandardError.ReadToEndAsync()
  if(!$p.WaitForExit(180000)){$p.Kill();throw "Timed out: $name"};$p.WaitForExit()
  $code=$p.ExitCode
  [IO.File]::WriteAllText($out,$stdout.GetAwaiter().GetResult())
  [IO.File]::WriteAllText($err,$stderr.GetAwaiter().GetResult())
  $results.Add([pscustomobject]@{name=$name;exit=$code})
  if($code -ne 0){throw "Failed: $name (see logs)"}
 }finally{$p.Dispose()}
}
try{
 $os=Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
 $boot=Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\SecureBoot\State' -ErrorAction SilentlyContinue
 [pscustomobject]@{ProductName=$os.ProductName;DisplayVersion=$os.DisplayVersion;Build=$os.CurrentBuild;UBR=$os.UBR;SecureBoot=$boot.UEFISecureBootEnabled;PowerShell=$PSVersionTable.PSVersion.ToString()}|ConvertTo-Json|Set-Content "$root\os.json"
 if($os.CurrentBuild -ne '19045'){throw 'Expected Windows 10 22H2 build 19045'}
 foreach($spec in @(@('EQ','eq','SoundCurrent EQ',$EqRevision),@('STUDIO','studio','SoundCurrent Studio',$StudioRevision))){
  $kind=$spec[0];$slug=$spec[1];$name=$spec[2];$revision=$spec[3]
  $media=Join-Path $MediaRoot $kind
  $installer=Get-ChildItem "$media\dist\*-setup.exe"|Select-Object -First 1
  if(!$installer){throw 'Missing installer'}
  Run $installer.FullName '/S' "$slug-install"
  $app=Join-Path $env:LOCALAPPDATA "Programs\$name"
  $exe=Join-Path $app "soundcurrent-$slug.exe"
  if(!(Test-Path $exe)){throw 'Installed app missing'}
  $expected=Get-FileHash "$media\build-windows-native\package\soundcurrent-$slug.exe"
  $actual=Get-FileHash $exe
  if($actual.Hash -ne $expected.Hash){throw 'Installed app differs from CI payload'}
  [pscustomobject]@{revision=$revision;appSHA256=$actual.Hash;installerSHA256=(Get-FileHash $installer.FullName).Hash}|ConvertTo-Json|Set-Content "$root\$slug-provenance.json"
  $env:PATH=$app+';'+$env:PATH
  $env:QT_QPA_PLATFORM='windows'
  $env:QT_PLUGIN_PATH=$app
  $env:QT_QPA_PLATFORM_PLUGIN_PATH=Join-Path $app 'platforms'
  $env:SOUNDCURRENT_TEST_FONT_SCALE='1.5'
  $env:SOUNDCURRENT_UI_SCREENSHOT_DIR="$root\$slug-screenshots"
  Run $exe '--ui-self-test' "$slug-ui"
  foreach($test in 'soundcurrent-microphone-access-test','soundcurrent-windows-microphone-privacy-test','soundcurrent-responsive-ui-test'){
   Run "$media\build-windows-native\Release\$test.exe" ' ' "$slug-$test"
  }
  Run $exe '--localization-ui-test --language ar' "$slug-ar"
  Run "$app\uninstall.exe" '/S' "$slug-uninstall"
  for($i=0;$i -lt 40 -and (Test-Path $exe);$i++){Start-Sleep -Milliseconds 250}
  if(Test-Path $exe){throw 'Uninstall left app executable'}
  # Reinstall/uninstall proves in-place ordinary package lifecycle, not audio routing.
  Run $installer.FullName '/S' "$slug-reinstall"
  Run $installer.FullName '/S' "$slug-update"
  Run "$app\uninstall.exe" '/S' "$slug-final-uninstall"
  for($i=0;$i -lt 40 -and (Test-Path $exe);$i++){Start-Sleep -Milliseconds 250}
  if(Test-Path $exe){throw 'Final uninstall left app executable'}
  $regName=if($slug -eq 'eq'){'SoundCurrentEQ'}else{'SoundCurrentStudio'}
  if(Test-Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$regName"){throw 'Final uninstall left uninstall registration'}
 }
 & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$MediaRoot\tests\windows_driver_signing_inputs.ps1" *> "$root\signing-inputs.txt"
 if($LASTEXITCODE -ne 0){throw 'Signing-input regression failed'}
}catch{$failure=$_.Exception.Message}
[pscustomobject]@{success=($null -eq $failure);failure=$failure;tests=@($results.ToArray());started=$started;finished=(Get-Date);limitations='No VB-CABLE/native driver installation, real microphone capture, or hardware audio latency tested. UI font scaling is not physical mixed-DPI coverage.'}|ConvertTo-Json -Depth 6|Set-Content "$root\result.json"
# One-shot serial transfer to the owning libvirt console, no network listener.
$zip="$env:TEMP\soundcurrent-win10-report.zip"
if(Test-Path $zip){Remove-Item $zip}
Compress-Archive -Path "$root\*.json","$root\*.txt" -DestinationPath $zip
$port=New-Object System.IO.Ports.SerialPort 'COM1',115200,([System.IO.Ports.Parity]::None),8,([System.IO.Ports.StopBits]::One)
try{$port.WriteTimeout=10000;$port.Open();$port.WriteLine('SOUNDCURRENT_REPORT_BEGIN');$port.WriteLine([Convert]::ToBase64String([IO.File]::ReadAllBytes($zip)));$port.WriteLine('SOUNDCURRENT_REPORT_END');$deadline=[DateTime]::UtcNow.AddSeconds(10);while($port.BytesToWrite -gt 0){if([DateTime]::UtcNow -gt $deadline){throw 'Serial drain timed out'};Start-Sleep -Milliseconds 50};$port.BaseStream.Flush()}finally{$port.Close()}
if($failure){Write-Host "FAIL: $failure";exit 1};Write-Host 'PASS: ordinary app lifecycle and privacy/layout tests. Native Windows 10 driver support remains unverified.'
