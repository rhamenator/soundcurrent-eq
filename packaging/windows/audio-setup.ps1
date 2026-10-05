# SPDX-License-Identifier: GPL-3.0-only
param([switch]$Check, [switch]$Install, [switch]$Quiet)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# Exit codes: 0 = present, 10 = absent, 20 = check failed,
# 30 = cancelled/failed installation, 3010 = installed; restart required.
function Find-Cable {
    # Match the standard cable's hardware ID, not a user-editable endpoint name.
    @(Get-CimInstance Win32_PnPEntity -Filter "Service='VBAudioVACMME'" |
        Where-Object { $_.HardwareID -contains 'VBAudioVACWDM' }).Count -gt 0
}
function Show-Notice([string]$Text) {
    if ($Quiet) { Write-Output $Text; return }
    if (-not ('SoundCurrentSetupNotice' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SoundCurrentSetupNotice {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern int MessageBoxW(IntPtr owner, string text, string caption, uint flags);
}
'@
    }
    # Native foreground dialog; do not let a hidden PowerShell console own it.
    [void][SoundCurrentSetupNotice]::MessageBoxW([IntPtr]::Zero, $Text, 'SoundCurrent EQ audio setup', 0x10040)
}

try {
    $present = Find-Cable
    if ($Check) {
        if ($present) { exit 0 }
        exit 10
    }
    if (-not $Install) { throw 'Choose -Check or -Install.' }
    if ($present) {
        Show-Notice 'VB-CABLE is already installed. Its installer was skipped to avoid removing the existing driver.'
        exit 0
    }

    $archive = Join-Path $PSScriptRoot 'VBCABLE_Driver_Pack45.zip'
    $expected = 'b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb'
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) {
        throw 'The bundled VB-CABLE package failed its integrity check. Download a fresh SoundCurrent EQ installer.'
    }
    $working = Join-Path ([IO.Path]::GetTempPath()) ('SoundCurrent-Cable-' + [Guid]::NewGuid().ToString('N'))
    try {
        Expand-Archive -LiteralPath $archive -DestinationPath $working
        $setup = Join-Path $working 'VBCABLE_Setup_x64.exe'
        if ((Get-AuthenticodeSignature -LiteralPath $setup).Status -ne 'Valid') {
            throw 'Windows could not verify the VB-Audio installer signature. Check your system clock and certificate updates, then retry.'
        }
        # Only the vendor's signed installer is elevated. Our app stays per-user.
        Start-Process -FilePath $setup -WorkingDirectory $working -Verb RunAs -Wait
        if (-not (Find-Cable)) {
            Show-Notice 'VB-CABLE was not installed. You can retry from Start > SoundCurrent EQ > Install VB-CABLE. Audio processing requires the cable driver.'
            exit 30
        }
        Show-Notice 'VB-CABLE is installed. Restart Windows before using SoundCurrent EQ. After restarting, select CABLE Input as the Windows output and your speakers or headphones in SoundCurrent EQ.'
        exit 3010
    } finally {
        if ($working -and (Test-Path -LiteralPath $working)) {
            Remove-Item -LiteralPath $working -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
} catch {
    if ($Check) { Write-Output $_.Exception.Message; exit 20 }
    Show-Notice ('Audio driver setup did not finish: ' + $_.Exception.Message + "`r`n`r`nYou can retry from the Start menu. SoundCurrent EQ itself remains installed.")
    exit 30
}
