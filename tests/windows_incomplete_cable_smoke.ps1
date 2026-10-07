# SPDX-License-Identifier: GPL-3.0-only
# System-changing fixture: temporarily disables the primary cable in an independent Windows clone.
param([switch]$Run)
if(!$Run){Write-Output "Use -Run only in an independent Windows clone.";exit 0}
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root="$env:USERPROFILE\SoundCurrent-integrated-build\soundcurrent-eq"
$stage=Join-Path $env:TEMP ('SoundCurrent-Incomplete-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $stage|Out-Null
Copy-Item "$root\package\*" $stage -Recurse
Copy-Item "$root\packaging\windows\cable-setup.ps1" "$stage\audio-setup.ps1"
Copy-Item "$root\packaging\windows\cable-setup.ps1" "$stage\cable-setup.ps1"
Copy-Item "$env:USERPROFILE\sc-cable.zip" "$stage\VBCABLE_Driver_Pack45.zip"
$endpoint=Get-CimInstance Win32_PnPEntity | Where-Object { $_.HardwareID -contains 'VBAudioVACWDM' -or $_.HardwareID -contains 'ROOT\VBAudioVACWDM' } | ForEach-Object { Get-PnpDevice -InstanceId $_.DeviceID }
if(@($endpoint).Count -ne 1){throw 'Need one healthy primary capture endpoint'}
try {
 Disable-PnpDevice -InstanceId $endpoint.InstanceId -Confirm:$false
 Start-Sleep -Seconds 2
 & "$stage\soundcurrent-cable-setup-guard.exe" --check-ready
 if($LASTEXITCODE -ne 10){throw "Readiness did not reject disabled endpoint: $LASTEXITCODE"}
 & powershell.exe -NoProfile -File "$stage\audio-setup.ps1" -Check
 if($LASTEXITCODE -ne 11){throw "Incomplete installation not detected: $LASTEXITCODE"}
 $env:QT_QPA_PLATFORM='offscreen'
 $p=Start-Process "$stage\soundcurrent-eq.exe" -ArgumentList '--windows-audio-setup-test settings "driver record"' -PassThru -Wait
 if($p.ExitCode -ne 0){throw "Incomplete settings UI failed $($p.ExitCode)"}
 'PASS: retained PnP driver with disabled capture endpoint rejected as incomplete; actual settings button reports repair and remains open'
} finally {
 Enable-PnpDevice -InstanceId $endpoint.InstanceId -Confirm:$false
 Start-Sleep -Seconds 2
 Remove-Item $stage -Recurse -Force
}
& "$root\package\soundcurrent-cable-setup-guard.exe" --check-ready
if($LASTEXITCODE -ne 0){throw 'Endpoint readiness not restored'}
'PASS: primary endpoints restored after independent-clone fixture'
