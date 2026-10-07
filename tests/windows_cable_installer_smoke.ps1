# SPDX-License-Identifier: GPL-3.0-only
# Opt-in system-changing test: run only on a full independent Windows VM clone.
param([switch]$Run, [string]$InstallerDirectory = $env:USERPROFILE)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
if (!$Run) { Write-Output 'Use -Run only on an independent Windows test clone.'; exit 0 }

function AudioDevices {
    @(Get-CimInstance Win32_PnPEntity -Filter "PNPClass='MEDIA'" |
        Sort-Object DeviceID | Select-Object DeviceID,Service,Status,ConfigManagerErrorCode) | ConvertTo-Json -Compress
}
function Wait-ProcessCode([string]$File, [string[]]$Arguments) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $File
    # Callers supply only fixed single-token switches (/S or --ui-self-test).
    $info.Arguments = $Arguments -join ' '
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $info
    try {
        if (!$process.Start()) { throw "Launch failed: $File" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(180000)) { $process.Kill(); throw "Timed out: $File" }
        Write-Output $stdout.Result
        Write-Output $stderr.Result
        $code = $process.ExitCode
        if ($null -eq $code -or $code -ne 0) { throw "Failed ($code): $File" }
    } finally { $process.Dispose() }
}
$before = AudioDevices
foreach ($item in @(
    @{name='EQ'; product='SoundCurrentEQ'; executable='soundcurrent-eq.exe'},
    @{name='Studio'; product='SoundCurrentStudio'; executable='soundcurrent-studio.exe'}
)) {
    $installer = Join-Path $InstallerDirectory $(if ($item.name -eq 'EQ') { 'SoundCurrent-EQ-0.7.6-windows-x64-VBCABLE-preview.exe' } else { 'SoundCurrent-Studio-0.8.5-windows-x64-VBCABLE-preview.exe' })
    if (!(Test-Path -LiteralPath $installer)) { throw "Missing fixture: $installer" }
    $key = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$($item.product)"
    # Silent mode installs the application without requesting driver elevation.
    # Its unsigned package is subsequently tested for explicit refusal.
    Wait-ProcessCode $installer @('/S')
    $directory = Split-Path (Get-ItemProperty -LiteralPath $key).DisplayIcon -Parent
    foreach ($file in @($item.executable,'soundcurrent-route-guardian.exe','soundcurrent-cable-setup-guard.exe',
        'LICENSE','THIRD-PARTY-NOTICES.md','licenses\Qt\LICENSES\GPL-3.0-only.txt',
        'licenses\Qt\LICENSES\LGPL-3.0-only.txt','licenses\Qt\LICENSES\BSD-3-Clause.txt',
        'audio-setup.ps1','cable-setup.ps1','VBCABLE_Driver_Pack45.zip','VB-CABLE-NOTICE.txt')) {
        if (!(Test-Path -LiteralPath (Join-Path $directory $file))) { throw "Incomplete installed payload: $file" }
    }
    $marker = Join-Path $directory 'soundcurrent-installer-test-user-file.txt'
    'user-file-preserved' | Set-Content -LiteralPath $marker
    Wait-ProcessCode $installer @('/S')
    if ((Get-Content -LiteralPath $marker) -ne 'user-file-preserved') { throw 'In-place update lost user file' }
    & powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File "$directory\audio-setup.ps1" -Install -Quiet
    if ($LASTEXITCODE -ne 0) { throw 'Installed setup did not reuse existing signed VB-CABLE' }
    if ((AudioDevices) -ne $before) { throw 'Installer changed hardware drivers despite existing-cable reuse' }

    if (Get-Process VBCABLE_ControlPanel -ErrorAction SilentlyContinue) { throw 'Close VB-CABLE control panel before testing' }
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CableTestWindow {
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,int m,IntPtr p,IntPtr l);
}
'@ -ErrorAction SilentlyContinue
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
    $info.Arguments = '-NoProfile -ExecutionPolicy RemoteSigned -File "'+$directory+'\cable-setup.ps1" -Settings -Quiet'
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $settings = New-Object System.Diagnostics.Process
    $settings.StartInfo = $info
    try {
        if (!$settings.Start()) { throw 'Cannot start cable settings' }
        $out = $settings.StandardOutput.ReadToEndAsync()
        $err = $settings.StandardError.ReadToEndAsync()
        $deadline = [DateTime]::UtcNow.AddSeconds(20)
        $panel = $null
        do {
            $panel = Get-Process VBCABLE_ControlPanel -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($panel) { $panel.Refresh(); if ($panel.MainWindowHandle -ne 0) { break } }
            if ($settings.HasExited) { throw "Settings exited before showing panel: $($err.Result) $($out.Result)" }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $deadline)
        if (!$panel -or $panel.MainWindowHandle -eq 0) { throw 'Real VB-CABLE control panel window missing' }
        Write-Output "PASS: $($item.name) VB-CABLE settings opened: $($panel.MainWindowTitle)"
        [void][CableTestWindow]::PostMessage($panel.MainWindowHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
        if (!$settings.WaitForExit(10000)) { throw 'Settings helper did not exit after panel closed' }
        if ($null -eq $settings.ExitCode -or $settings.ExitCode -ne 0) { throw "Settings helper failed: $($out.Result) $($err.Result)" }
    } finally {
        if (!$settings.HasExited) { $settings.Kill(); $settings.WaitForExit() }
        if ($panel -and !$panel.HasExited) { Stop-Process -Id $panel.Id -Force }
        $settings.Dispose()
    }

    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_PLUGIN_PATH = $directory
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $directory 'platforms'
    Wait-ProcessCode (Join-Path $directory $item.executable) @('--ui-self-test')
    Remove-Item Env:\QT_QPA_PLATFORM
    # NSIS relocates its uninstaller; wait for the real uninstaller's registry
    # removal as well as the launcher process, rather than treating launch as exit.
    Wait-ProcessCode (Join-Path $directory 'uninstall.exe') @('/S')
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    while ((Test-Path -LiteralPath $key) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $key) { throw 'Uninstaller registration remains' }
    if (Test-Path -LiteralPath (Join-Path $directory $item.executable)) { throw 'Uninstaller left app executable' }
    if (Test-Path -LiteralPath (Join-Path $directory 'soundcurrent-driver-manager.exe')) { throw 'Uninstaller left driver manager' }
    if (!(Test-Path -LiteralPath $marker)) { throw 'Uninstaller deleted an unknown user file' }
    $licenses = Join-Path $directory 'licenses'
    if ((Test-Path -LiteralPath $licenses) -and !(Get-ChildItem -LiteralPath $licenses -Force)) { throw 'Uninstaller left an empty payload license directory' }
    Remove-Item -LiteralPath $marker
    if (!(Get-ChildItem -LiteralPath $directory -Force -ErrorAction SilentlyContinue)) { Remove-Item -LiteralPath $directory }
    Write-Output "PASS: $($item.name) install, in-place update, installed UI, existing-cable reuse and silent shared-driver retention"
}
if ((AudioDevices) -ne $before) { throw 'Application lifecycle changed physical/third-party audio devices' }
Write-Output 'PASS: physical and third-party audio device identities remained unchanged'
