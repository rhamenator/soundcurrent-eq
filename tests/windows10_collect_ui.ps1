# SPDX-License-Identifier: GPL-3.0-only
# Read only the known synthetic UI-test screenshots in an owned disposable VM.
param([switch]$DisposableVM,[Parameter(Mandatory=$true)][string]$Root)
$ErrorActionPreference='Stop'
if(!$DisposableVM -or (Get-CimInstance Win32_ComputerSystem).Manufacturer -notmatch '^QEMU$'){throw 'Disposable QEMU VM only'}
$Root=(Resolve-Path -LiteralPath $Root).Path
if(!$Root.StartsWith($env:USERPROFILE+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Expected a user-owned test result directory'}
if(!(Get-Content "$Root\result.json" -Raw|ConvertFrom-Json).success){throw 'Expected a successful synthetic test report'}
$temp=Join-Path $env:TEMP ('SoundCurrent-ui-export-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $temp|Out-Null
try{
 foreach($app in 'eq','studio'){
  foreach($name in 'equalizer','settings','studio','localized-tab-0','localized-tab-1','localized-tab-2'){
   $path=Join-Path $Root "$app-screenshots\$name.png"
   if(Test-Path $path){Copy-Item -LiteralPath $path -Destination "$temp\$app-$name.png"}
  }
 }
 $zip="$temp.zip";Compress-Archive -Path "$temp\*.png" -DestinationPath $zip
 $port=New-Object System.IO.Ports.SerialPort 'COM1',115200,([System.IO.Ports.Parity]::None),8,([System.IO.Ports.StopBits]::One)
 try{
  $port.WriteTimeout=10000;$port.Open();$port.WriteLine('SOUNDCURRENT_REPORT_BEGIN')
  $port.WriteLine([Convert]::ToBase64String([IO.File]::ReadAllBytes($zip)))
  $port.WriteLine('SOUNDCURRENT_REPORT_END')
  $until=[DateTime]::UtcNow.AddSeconds(10)
  while($port.BytesToWrite -gt 0){if([DateTime]::UtcNow -gt $until){throw 'Serial drain timed out'};Start-Sleep -Milliseconds 50}
  $port.BaseStream.Flush()
 }finally{$port.Close()}
}finally{Remove-Item $temp -Recurse -Force -ErrorAction SilentlyContinue;Remove-Item "$temp.zip" -ErrorAction SilentlyContinue}
