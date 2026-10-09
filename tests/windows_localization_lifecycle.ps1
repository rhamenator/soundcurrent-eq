# SPDX-License-Identifier: GPL-3.0-only
# System-changing: run only on a full independent Windows test clone.
param([switch]$Run,[Parameter(Mandatory=$true)][ValidateSet('EQ','Studio')][string]$Product,
 [Parameter(Mandatory=$true)][string]$InstallerPath,[Parameter(Mandatory=$true)][string]$ResultDirectory)
$ErrorActionPreference='Stop'
if(!$Run){Write-Output 'Use -Run only on an independent Windows clone.';exit 0}
function Assert($value,$message){if(!$value){throw $message}}
function Execute([string]$file,[string]$arguments){
 $p=Start-Process -FilePath $file -ArgumentList $arguments -PassThru
 try {if(!$p.WaitForExit(90000)){Stop-Process -Id $p.Id -Force;throw 'Installer timed out'}
 $p.Refresh();Assert ($p.ExitCode -eq 0) "Installer failed: $($p.ExitCode)"
 }finally{$p.Dispose()}
}
function AudioState {
 @(Get-CimInstance Win32_PnPEntity -Filter "PNPClass='MEDIA'" | Sort-Object DeviceID |
 Select-Object DeviceID,Service,Status,ConfigManagerErrorCode) | ConvertTo-Json -Compress
}
$name='soundcurrent-'+$Product.ToLower()
Assert (-not (Get-Process $name -ErrorAction SilentlyContinue)) 'Quit the tested application first'
$uninstallKey="HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrent$Product"
Assert (Test-Path $uninstallKey) 'Install the candidate before this lifecycle test'
$directory=Split-Path (Get-ItemProperty $uninstallKey).DisplayIcon -Parent
$exe=Join-Path $directory "$name.exe"
Assert (Test-Path $exe) 'Installed executable missing'
New-Item -ItemType Directory -Force $ResultDirectory | Out-Null
$before=AudioState
$settings="HKCU:\Software\SoundCurrent\$name"
if(!(Test-Path $settings)){New-Item $settings | Out-Null}
$sentinel='localizationQaPreservation_'+[guid]::NewGuid().ToString('N')
New-ItemProperty -Path $settings -Name $sentinel -Value 'preserve-settings' -PropertyType String | Out-Null
$marker=Join-Path $directory ('localization-qa-user-'+[guid]::NewGuid().ToString('N')+'.txt')
'preserve-user-file' | Set-Content -Encoding UTF8 $marker
try {
 Execute $InstallerPath '/S'
 Assert ((Get-ItemPropertyValue $settings $sentinel) -eq 'preserve-settings') 'Update lost settings marker'
 Assert ((Get-Content $marker) -eq 'preserve-user-file') 'Update lost unknown user file'
 Assert ((AudioState) -eq $before) 'Silent update changed audio device identities/status'
 Execute (Join-Path $directory 'uninstall.exe') '/S'
 $deadline=[DateTime]::UtcNow.AddSeconds(30)
 while((Test-Path $uninstallKey) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100}
 Assert (-not (Test-Path $uninstallKey)) 'Uninstaller registration remains'
 Assert (-not (Test-Path $exe)) 'Uninstaller left application executable'
 Assert ((Get-ItemPropertyValue $settings $sentinel) -eq 'preserve-settings') 'Uninstall lost settings marker'
 Assert ((Get-Content $marker) -eq 'preserve-user-file') 'Uninstall deleted unknown user file'
 Assert ((AudioState) -eq $before) 'Silent uninstall changed audio device identities/status'
 @{product=$Product;installerSha256=(Get-FileHash $InstallerPath).Hash;update='passed';uninstall='passed';settingsMarkerPreserved=$true;unknownFilePreserved=$true;audioDeviceStateUnchanged=$true;scope='Silent per-user app lifecycle on independent clone; no driver installation/removal or visual qualification';nativeReviewed=$false} | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $ResultDirectory 'lifecycle.json')
 Write-Output "PASS: $Product update/uninstall, settings/user-file preservation and audio-device state"
}finally {
 Remove-ItemProperty $settings -Name $sentinel -ErrorAction SilentlyContinue
 Remove-Item -LiteralPath $marker -ErrorAction SilentlyContinue
}
