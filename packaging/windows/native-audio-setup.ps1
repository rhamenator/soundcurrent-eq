# SPDX-License-Identifier: GPL-3.0-only
param([switch]$Check, [switch]$Install, [switch]$Remove, [switch]$Quiet,
      [ValidateSet('eq','studio')][string]$App = 'eq', [string]$Language = '')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# Match the app's UTF-8 QProcess output decoder, including Windows PowerShell 5.1.
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
$localization = Join-Path $PSScriptRoot 'setup-localization.ps1'
if (Test-Path -LiteralPath $localization) { . $localization }
else {
    function Get-SCSetupText([string]$Source, [string]$Language = '', [string]$Application = '') { return $Source }
    function Format-SCSetupText([string]$Source, [string[]]$Values, [string]$Language = '', [string]$Application = '') {
        # Older/missing payload fallback: format owned English templates once.
        $replacement = {param($match) return [string]$Values[[int]$match.Value.Substring(1) - 1]}.GetNewClosure()
        return [regex]::Replace($Source,'%[1-9][0-9]*',[Text.RegularExpressions.MatchEvaluator]$replacement)
    }
}

function Notice([string]$Text) {
    if ($Quiet) { Write-Output $Text; return }
    Add-Type -AssemblyName System.Windows.Forms
    [void][System.Windows.Forms.MessageBox]::Show($Text, (Get-SCSetupText 'Audio driver setup' -Language $Language -Application ('soundcurrent-' + $App)))
}
try {
    if (@($Check,$Install,$Remove).Where({$_}).Count -ne 1) {
        throw (Get-SCSetupText 'Choose one audio setup action.' -Language $Language -Application ('soundcurrent-' + $App))
    }
    if ($Check) {
        # This can run from the installer's temporary directory before payload
        # extraction. Hardware identity is independent of endpoint renaming.
        $present = @(Get-CimInstance Win32_PnPEntity -Filter "Service='SoundCurrentVAD'" |
            Where-Object { $_.HardwareID -contains 'Root\SOUNDCURRENTVAD' }).Count -gt 0
        if ($present) { exit 0 }
        exit 10
    }
    $helper = Join-Path $PSScriptRoot 'soundcurrent-driver-manager.exe'
    if (!(Test-Path -LiteralPath $helper)) { throw (Get-SCSetupText 'The shared driver manager is missing. Repair the app installation.' -Language $Language -Application ('soundcurrent-' + $App)) }
    # Resolve before UAC: a different administrator can approve setup without
    # becoming the registered owner of the requesting user's app installation.
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    if ($Remove) {
        # NSIS may launch 32-bit PowerShell; ownership always uses HKLM64.
        $registry = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryView]::Registry64)
        try {
            $owners = $registry.OpenSubKey('SOFTWARE\SoundCurrent\AudioDriver\Owners')
            try { $owned = $owners -and ($null -ne $owners.GetValue("$sid.$App")) }
            finally { if ($owners) { $owners.Dispose() } }
        } finally { $registry.Dispose() }
        if (!$owned) { exit 0 }
    }
    # The in-app setup action quits the app after starting this wrapper. Wait
    # briefly for its route guardian to finish before requesting elevation.
    $until = [DateTime]::UtcNow.AddSeconds(15)
    while (Get-Process -Name 'soundcurrent-eq','soundcurrent-studio','soundcurrent-route-guardian' -ErrorAction SilentlyContinue) {
        if ([DateTime]::UtcNow -ge $until) { throw (Get-SCSetupText 'Quit running SoundCurrent apps and wait for audio recovery to finish before changing the shared audio driver.' -Language $Language -Application ('soundcurrent-' + $App)) }
        Start-Sleep -Milliseconds 200
    }
    $package = Join-Path $PSScriptRoot 'audio-driver'
    if ($Install) {
        & $helper --verify $package
        if ($LASTEXITCODE -ne 0) { throw (Get-SCSetupText 'The driver package is incomplete or Windows cannot verify its signature.' -Language $Language -Application ('soundcurrent-' + $App)) }
    }
    # Production setup elevates only the signed manager. Unsigned local builds
    # can check status/package rejection, but cannot request privileged setup.
    if ((Get-AuthenticodeSignature -LiteralPath $helper).Status -ne 'Valid') {
        throw (Get-SCSetupText 'The driver manager is not signed. Install a signed SoundCurrent release.' -Language $Language -Application ('soundcurrent-' + $App))
    }
    if ($Install) { $arguments = @('--install',$App,('"{0}"' -f $package),$sid) }
    else { $arguments = @('--remove',$App,$sid) }
    $process = Start-Process -FilePath $helper -ArgumentList $arguments -Verb RunAs -Wait -PassThru
    $code = $process.ExitCode
    if ($code -notin @(0,3010)) { throw (Format-SCSetupText 'Driver setup failed (code %1). No Windows security settings were changed.' -Values @([string]$code) -Language $Language -Application ('soundcurrent-' + $App)) }
    if ($code -eq 3010) { Notice (Get-SCSetupText 'Audio driver setup completed. Restart Windows before using SoundCurrent.' -Language $Language -Application ('soundcurrent-' + $App)) }
    elseif ($Install) { Notice (Get-SCSetupText 'SoundCurrent Audio is ready. Open the app and choose your speakers or headphones.' -Language $Language -Application ('soundcurrent-' + $App)) }
    exit $code
} catch {
    if ($Check) { Write-Output $_.Exception.Message; exit 20 }
    Notice (Format-SCSetupText 'Audio driver setup did not finish: %1' -Values @($_.Exception.Message) -Language $Language -Application ('soundcurrent-' + $App))
    exit 30
}
