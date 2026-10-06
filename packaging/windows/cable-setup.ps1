# SPDX-License-Identifier: GPL-3.0-only
param([switch]$Check, [switch]$Install, [switch]$Remove, [switch]$Settings,
      [switch]$Quiet, [switch]$Silent,
      [ValidateSet('eq','studio')][string]$App = 'eq')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
function Notice([string]$Text) {
    Write-Output $Text
    if (!$Quiet) {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show($Text,'SoundCurrent audio setup')
    }
}
function Present {
    # Match the primary VB-CABLE hardware ID, never other VB-Audio products.
    return @(Get-CimInstance Win32_PnPEntity | Where-Object {
        $_.HardwareID -contains 'VBAudioVACWDM' -or $_.HardwareID -contains 'ROOT\VBAudioVACWDM'
    }).Count -gt 0
}
try {
    if (@($Check,$Install,$Remove,$Settings).Where({$_}).Count -ne 1) { throw 'Choose one audio setup action.' }
    if ($Check) { if (Present) { exit 0 }; exit 10 }
    if ($Silent -and ($Install -or $Remove)) { exit 0 } # No unattended third-party changes.
    if ($Remove) {
        if (!(Present)) { exit 0 }
        $other = if ($App -eq 'eq') { 'SoundCurrentStudio' } else { 'SoundCurrentEQ' }
        if (Test-Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$other") {
            Notice 'VB-CABLE was kept because the other SoundCurrent app is installed. Remove it with the last app if no other software needs it.'
            exit 0
        }
        Add-Type -AssemblyName System.Windows.Forms
        $answer = [System.Windows.Forms.MessageBox]::Show(
            'Remove the shared VB-CABLE driver too? Other users, recording apps, or voice tools may need it. Choose Yes to open the official remover, then click Remove Driver. Choose No to keep the cable and uninstall only SoundCurrent.',
            'Remove VB-CABLE?', 'YesNo', 'Question')
        if ($answer -ne 'Yes') { exit 0 }
    }
    if (!$Settings) {
        $until = [DateTime]::UtcNow.AddSeconds(15)
        while (Get-Process -Name 'soundcurrent-eq','soundcurrent-studio','soundcurrent-route-guardian' -ErrorAction SilentlyContinue) {
            if ([DateTime]::UtcNow -ge $until) { throw 'Quit both SoundCurrent apps before changing VB-CABLE.' }
            Start-Sleep -Milliseconds 200
        }
        if ($Install -and (Present)) { Notice 'VB-CABLE is already installed. Open SoundCurrent and select your speakers. Use VB-CABLE settings for its control panel.'; exit 0 }
    }
    $archive = Join-Path $PSScriptRoot 'VBCABLE_Driver_Pack45.zip'
    if (!(Test-Path -LiteralPath $archive)) { throw 'The VB-CABLE package is missing. Repair the SoundCurrent installation.' }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb') {
        throw 'VB-CABLE package checksum mismatch. Repair the installation.'
    }
    # Use a fresh private extraction each time; do not trust cached executables.
    $temp = Join-Path ([IO.Path]::GetTempPath()) ('SoundCurrent-VBCABLE-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $temp | Out-Null
    try {
        Expand-Archive -LiteralPath $archive -DestinationPath $temp
        $exe = Join-Path $temp $(if ($Settings) { 'VBCABLE_ControlPanel.exe' } else { 'VBCABLE_Setup_x64.exe' })
        if ((Get-AuthenticodeSignature -LiteralPath $exe).Status -ne 'Valid') { throw 'Windows could not verify the VB-Audio executable signature.' }
        if ($Settings) {
            $process = Start-Process -FilePath $exe -WorkingDirectory $temp -PassThru
            $process.WaitForExit()
            if ($process.ExitCode -ne 0) { throw 'VB-CABLE control panel failed.' }
        } else {
            $guard = Join-Path $PSScriptRoot 'soundcurrent-cable-setup-guard.exe'
            if (!(Test-Path -LiteralPath $guard)) { throw 'The route-preserving setup helper is missing.' }
            & $guard $(if ($Install) { '--install' } else { '--remove' }) $exe
            if ($LASTEXITCODE -ne 0) { throw "VB-CABLE setup was cancelled or did not finish (code $LASTEXITCODE). SoundCurrent was retained for retry." }
            if ($Remove -and (Present)) { throw 'VB-CABLE is still present. If removal requested a restart, restart Windows and retry SoundCurrent uninstall; otherwise finish Remove Driver in the official setup.' }
            if ($Install -and !(Present)) { throw 'VB-CABLE is not present. Restart Windows if requested, then retry audio setup.' }
            Notice 'VB-CABLE setup finished. Restart Windows as requested by VB-Audio. Your prior audio defaults were preserved where still available.'
            exit 3010
        }
    } finally { if ($temp) { Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue } }
    exit 0
} catch {
    if ($Check) { Write-Output $_.Exception.Message; exit 20 }
    Notice $_.Exception.Message
    exit 30
}
