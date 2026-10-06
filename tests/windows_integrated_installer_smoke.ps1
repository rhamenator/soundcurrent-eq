# SPDX-License-Identifier: GPL-3.0-only
# Opt-in system-changing test: run only on a full independent Windows VM clone.
param([switch]$Run, [string]$PrototypeDirectory = $env:USERPROFILE)
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
        if (!$process.WaitForExit(60000)) { $process.Kill(); throw "Timed out: $File" }
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
    $installer = Join-Path $PrototypeDirectory "SoundCurrent-$($item.name)-integrated-unsigned-prototype.exe"
    if (!(Test-Path -LiteralPath $installer)) { throw "Missing fixture: $installer" }
    $key = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$($item.product)"
    # Silent mode installs the application without requesting driver elevation.
    # Its unsigned package is subsequently tested for explicit refusal.
    Wait-ProcessCode $installer @('/S')
    $directory = Split-Path (Get-ItemProperty -LiteralPath $key).DisplayIcon -Parent
    foreach ($file in @($item.executable,'soundcurrent-route-guardian.exe','soundcurrent-driver-manager.exe',
        'LICENSE','THIRD-PARTY-NOTICES.md','licenses\Qt\LICENSES\GPL-3.0-only.txt',
        'licenses\Qt\LICENSES\LGPL-3.0-only.txt','licenses\Qt\LICENSES\BSD-3-Clause.txt',
        'audio-setup.ps1','audio-driver\soundcurrentvad.inf','audio-driver\soundcurrentvad.sys',
        'audio-driver\soundcurrentvad.cat','licenses\SoundCurrent-driver-MS-PL.txt')) {
        if (!(Test-Path -LiteralPath (Join-Path $directory $file))) { throw "Incomplete installed payload: $file" }
    }
    $marker = Join-Path $directory 'soundcurrent-installer-test-user-file.txt'
    'user-file-preserved' | Set-Content -LiteralPath $marker
    Wait-ProcessCode $installer @('/S')
    if ((Get-Content -LiteralPath $marker) -ne 'user-file-preserved') { throw 'In-place update lost user file' }
    & powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File "$directory\audio-setup.ps1" -Install -Quiet
    if ($LASTEXITCODE -ne 30) { throw 'Installed setup accepted unsigned driver package' }
    if ((AudioDevices) -ne $before) { throw 'Installer changed hardware drivers despite unsigned-package refusal' }
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
    Write-Output "PASS: $($item.name) install, in-place update, installed UI, unsigned-driver refusal and unowned-driver uninstall"
}
if ((AudioDevices) -ne $before) { throw 'Application lifecycle changed physical/third-party audio devices' }
Write-Output 'PASS: physical and third-party audio device identities remained unchanged'
