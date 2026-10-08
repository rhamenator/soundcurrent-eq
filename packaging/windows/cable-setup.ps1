# SPDX-License-Identifier: GPL-3.0-only
param([switch]$Check, [switch]$Install, [switch]$Remove, [switch]$Settings,
      [switch]$Quiet, [switch]$Silent, [int]$RequestingProcessId = 0,
      [ValidateSet('eq','studio')][string]$App = 'eq', [string]$Language = '')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# Match the app's UTF-8 QProcess output decoder, including Windows PowerShell 5.1.
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
$localization = Join-Path $PSScriptRoot 'setup-localization.ps1'
if (Test-Path -LiteralPath $localization) { . $localization }
else { function Get-SCSetupText([string]$Source, [string]$Language = '', [string]$Application = '') { return $Source } }
function Notice([string]$Text) {
    Write-Output $Text
    if (!$Quiet) {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show($Text,(Get-SCSetupText 'Audio driver setup' -Language $Language -Application ('soundcurrent-' + $App)))
    }
}
function Present {
    # Match the primary VB-CABLE hardware ID, never other VB-Audio products.
    return @(Get-CimInstance Win32_PnPEntity | Where-Object {
        $_.HardwareID -contains 'VBAudioVACWDM' -or $_.HardwareID -contains 'ROOT\VBAudioVACWDM'
    }).Count -gt 0
}
function Ready {
    $probe = Join-Path $PSScriptRoot 'soundcurrent-cable-setup-guard.exe'
    if (!(Test-Path -LiteralPath $probe)) { throw 'The audio readiness helper is missing. Repair the SoundCurrent installation.' }
    & $probe --check-ready
    return $LASTEXITCODE -eq 0
}
# A shared per-user boot marker prevents either app from treating newly
# installed cable endpoints as ready before the required Windows restart.
$rebootKey = 'HKCU:\Software\SoundCurrent\VBCable'
function BootStamp { return (Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().Ticks.ToString() }
function NeedsReboot {
    $stamp = (Get-ItemProperty -LiteralPath $rebootKey -Name InstalledDuringBoot -ErrorAction SilentlyContinue).InstalledDuringBoot
    if (!$stamp) { return $false }
    if ($stamp -eq (BootStamp)) { return $true }
    Remove-ItemProperty -LiteralPath $rebootKey -Name InstalledDuringBoot -ErrorAction SilentlyContinue
    return $false
}
function MarkReboot {
    New-Item -Path $rebootKey -Force | Out-Null
    Set-ItemProperty -LiteralPath $rebootKey -Name InstalledDuringBoot -Value (BootStamp)
}
function RunningClients {
    @(Get-Process -Name 'soundcurrent-eq','soundcurrent-studio','soundcurrent-route-guardian' -ErrorAction SilentlyContinue |
        Where-Object { $_.Id -ne $RequestingProcessId })
}
function QuitMessage($Clients) {
    $names = @($Clients | ForEach-Object {
        if ($_.Name -eq 'soundcurrent-eq') { 'SoundCurrent EQ' }
        elseif ($_.Name -eq 'soundcurrent-studio') { 'SoundCurrent Studio' }
        else { 'the audio recovery helper' }
    } | Select-Object -Unique)
    return 'Quit ' + ($names -join ' and ') + ' before changing VB-CABLE.'
}
try {
    if (@($Check,$Install,$Remove,$Settings).Where({$_}).Count -ne 1) { throw (Get-SCSetupText 'Choose one audio setup action.' -Language $Language -Application ('soundcurrent-' + $App)) }
    if ($Check) { if (NeedsReboot) { exit 3010 }; if (Present) { if (Ready) { exit 0 }; exit 11 }; exit 10 }
    if (!$Remove -and (NeedsReboot)) { Notice 'Restart Windows before using VB-CABLE. Audio setup has completed, but the driver and its settings require a system restart.'; exit 3010 }
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
    if ($RequestingProcessId) {
        $requester = Get-Process -Id $RequestingProcessId -ErrorAction Stop
        $expected = if ($App -eq 'eq') { 'soundcurrent-eq' } else { 'soundcurrent-studio' }
        if ($requester.Name -ne $expected -or $requester.SessionId -ne (Get-Process -Id $PID).SessionId) {
            throw (Get-SCSetupText 'Invalid audio setup requester.' -Language $Language -Application ('soundcurrent-' + $App))
        }
    }
    if (!$Settings) {
        $until = [DateTime]::UtcNow.AddSeconds(15)
        while ($clients = RunningClients) {
            if ([DateTime]::UtcNow -ge $until) { throw (QuitMessage $clients) }
            Start-Sleep -Milliseconds 200
        }
        if ($Install -and (Present) -and (Ready)) { Notice 'VB-CABLE is already installed. If it was just installed or updated, restart Windows before using the equalizer or VB-CABLE settings. Otherwise, select your speakers in SoundCurrent.'; exit 0 }
    }
    $archive = Join-Path $PSScriptRoot 'VBCABLE_Driver_Pack45.zip'
    if (!(Test-Path -LiteralPath $archive)) { throw (Get-SCSetupText 'The VB-CABLE package is missing. Repair the SoundCurrent installation.' -Language $Language -Application ('soundcurrent-' + $App)) }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb') {
        throw (Get-SCSetupText 'VB-CABLE package checksum mismatch. Repair the installation.' -Language $Language -Application ('soundcurrent-' + $App))
    }
    # Use a fresh private extraction each time; do not trust cached executables.
    $temp = Join-Path ([IO.Path]::GetTempPath()) ('SoundCurrent-VBCABLE-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $temp | Out-Null
    try {
        Expand-Archive -LiteralPath $archive -DestinationPath $temp
        $exe = Join-Path $temp $(if ($Settings) { 'VBCABLE_ControlPanel.exe' } else { 'VBCABLE_Setup_x64.exe' })
        if ((Get-AuthenticodeSignature -LiteralPath $exe).Status -ne 'Valid') { throw (Get-SCSetupText 'Windows could not verify the VB-Audio executable signature.' -Language $Language -Application ('soundcurrent-' + $App)) }
        if ($Settings) {
            if (!(Present)) { throw 'VB-CABLE is not installed. Use Audio driver setup, then restart Windows before opening its settings.' }
            if (!(Ready)) { throw 'Windows has a VB-CABLE driver record, but its playback or recording endpoint is unavailable. If you have already restarted, use Audio driver setup to repair it. Enable CABLE Input and CABLE Output in Windows Sound settings if they are disabled.' }
            $process = Start-Process -FilePath $exe -WorkingDirectory $temp -PassThru
            $process.WaitForExit()
            if ($process.ExitCode -ne 0) { throw 'VB-CABLE settings could not open. Restart Windows if the driver was just installed or updated, then try again.' }
        } else {
            $guard = Join-Path $PSScriptRoot 'soundcurrent-cable-setup-guard.exe'
            if (!(Test-Path -LiteralPath $guard)) { throw (Get-SCSetupText 'The route-preserving setup helper is missing.' -Language $Language -Application ('soundcurrent-' + $App)) }
            $repair = $Install -and (Present) -and !(Ready)
            if ($repair) {
                Add-Type -AssemblyName System.Windows.Forms
                [void][System.Windows.Forms.MessageBox]::Show(
                    'Windows has a VB-CABLE driver record but no usable cable endpoints. First check that CABLE Input and CABLE Output are enabled in Windows Sound settings. To reinstall: click Remove Driver in the official setup that opens next, restart Windows, then run Audio driver setup again and click Install Driver. Restart once more before using SoundCurrent. Removing this shared cable affects other apps that use it.',
                    'Repair incomplete VB-CABLE installation')
            }
            if ($Install) { MarkReboot } # Persist before mutation, even if the UI closes.
            & $guard $(if ($Install) { '--install' } else { '--remove' }) $exe
            if ($LASTEXITCODE -ne 0) { throw "VB-CABLE setup was cancelled or did not finish (code $LASTEXITCODE). SoundCurrent was retained for retry." }
            if ($Remove -and (Present)) { throw 'VB-CABLE is still present. If removal requested a restart, restart Windows and retry SoundCurrent uninstall; otherwise finish Remove Driver in the official setup.' }
            if ($repair -and !(Present)) {
                MarkReboot
                Notice 'The incomplete VB-CABLE installation was removed. Restart Windows, run Audio driver setup again, click Install Driver, then restart once more.'
                exit 3010
            }
            if ($Install -and !(Present)) { throw 'VB-CABLE is not present. Restart Windows if requested, then retry audio setup.' }
            if ($repair -and !(Ready)) { throw 'VB-CABLE still has no usable playback/recording endpoints. Complete Remove Driver in the official setup, restart Windows, then run Audio driver setup again to reinstall. Windows Sound settings must have CABLE Input and CABLE Output enabled.' }
            MarkReboot
            Notice 'VB-CABLE setup finished. Restart Windows now before using the equalizer or VB-CABLE settings. Your prior audio defaults were preserved where still available.'
            exit 3010
        }
    } finally { if ($temp) { Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue } }
    exit 0
} catch {
    $message = $_.Exception.Message
    try { if ($Install -and !(Present)) { Remove-ItemProperty -LiteralPath $rebootKey -Name InstalledDuringBoot -ErrorAction SilentlyContinue } } catch {}
    if ($Check) { Write-Output $message; exit 20 }
    Notice $message
    exit 30
}
