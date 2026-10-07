# SPDX-License-Identifier: GPL-3.0-only
# Run only in a signed-in, independent Windows test clone.
param([string]$InstallerPath, [string]$ResultPath = "$env:TEMP\soundcurrent-ui-result.txt")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class EqUi {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,int m,IntPtr p,IntPtr l);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w,out Rect r);
}
'@
function RunRegistry([string]$Arguments) {
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName="$env:SystemRoot\System32\reg.exe";$info.Arguments=$Arguments
    $info.UseShellExecute=$false;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $p=New-Object Diagnostics.Process;$p.StartInfo=$info
    try {
        [void]$p.Start();$o=$p.StandardOutput.ReadToEndAsync();$e=$p.StandardError.ReadToEndAsync()
        if(!$p.WaitForExit(10000)){$p.Kill();throw 'Registry test helper timed out'}
        if($p.ExitCode -ne 0){throw "Registry test helper failed: $($o.Result) $($e.Result)"}
    } finally {$p.Dispose()}
}
function Assert($Condition, $Message) { if (-not $Condition) { throw $Message } }
function WaitUntil([scriptblock]$Condition, [string]$Message) {
    $deadline=[DateTime]::UtcNow.AddSeconds(60)
    do { if (& $Condition) { return }; Start-Sleep -Milliseconds 100 } while ([DateTime]::UtcNow -lt $deadline)
    throw $Message
}
$key='HKCU\Software\SoundCurrent\soundcurrent-eq'
$backup=Join-Path $env:TEMP ('soundcurrent-settings-'+[guid]::NewGuid()+'.reg')
$hadSettings=Test-Path 'HKCU:\Software\SoundCurrent\soundcurrent-eq'
$app=$null
$settingsIsolated=$false
try {
    Assert (-not (Get-Process soundcurrent-eq -ErrorAction SilentlyContinue)) 'Quit the app before this test'
    if ($InstallerPath) {
        $installer=Start-Process $InstallerPath -ArgumentList '/S' -Wait -PassThru
        Assert ($installer.ExitCode -eq 0) 'Installer failed'
    }
    $exe="$env:LOCALAPPDATA\Programs\SoundCurrent EQ\soundcurrent-eq.exe"
    Assert (Test-Path $exe) 'Installed executable missing'
    Assert (Test-Path "$env:USERPROFILE\Desktop\SoundCurrent EQ.lnk") 'Desktop shortcut missing'
    Assert (Test-Path "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\SoundCurrent EQ\SoundCurrent EQ.lnk") 'Start menu shortcut missing'
    # Exercise the shared Qt controls using isolated temporary INI settings.
    $log=Join-Path $env:TEMP 'soundcurrent-ui-self-test.log'
    $env:QT_QPA_PLATFORM='windows'
    $test=Start-Process $exe -ArgumentList '--ui-self-test' -PassThru -RedirectStandardError $log
    $null=$test.Handle
    if (!$test.WaitForExit(90000)) { Stop-Process -Id $test.Id -Force; throw 'Shared UI test timed out' }
    $test.Refresh()
    Assert ($test.ExitCode -eq 0) "Shared UI test failed; see $log"
    Remove-Item Env:\QT_QPA_PLATFORM
    if ($hadSettings) { RunRegistry ('export "'+$key+'" "'+$backup+'" /y') }
    if ($hadSettings) { RunRegistry ('delete "'+$key+'" /f') }
    $settingsIsolated=$true
    $app=Start-Process $exe -PassThru
    WaitUntil { $script:window=[EqUi]::FindWindow([NullString]::Value,'SoundCurrent EQ'); $script:window -ne [IntPtr]::Zero } 'App window missing'
    WaitUntil { [EqUi]::IsWindowVisible($script:window) } 'App window is hidden'
    Add-Type -AssemblyName System.Windows.Forms
    WaitUntil {
        $rect=New-Object EqUi+Rect
        $ok=[EqUi]::GetWindowRect($script:window,[ref]$rect)
        $area=[System.Windows.Forms.Screen]::FromHandle($script:window).WorkingArea
        $ok -and $rect.Left -ge $area.Left -and $rect.Top -ge $area.Top -and $rect.Right -le $area.Right -and $rect.Bottom -le $area.Bottom
    } 'Decorated app window exceeds the desktop working area'
    [void][EqUi]::PostMessage($script:window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    WaitUntil { -not [EqUi]::IsWindowVisible($script:window) } 'Closing the window did not hide it'
    Assert (-not $app.HasExited) 'Closing the window unloaded the app'
    $second=Start-Process $exe -PassThru
    Assert ($second.WaitForExit(5000)) 'Second instance did not exit'
    WaitUntil { [EqUi]::IsWindowVisible($script:window) } 'Relaunch did not restore the window'
    $quit=Start-Process $exe -ArgumentList '--quit' -PassThru
    Assert ($quit.WaitForExit(5000)) 'Quit request failed'
    Assert ($app.WaitForExit(10000)) 'Quit did not unload the app'
    Assert (-not (Get-Process soundcurrent-eq -ErrorAction SilentlyContinue)) 'An equalizer process remains after quit'
    'PASS: installer shortcuts, shared Qt controls, window fit, background close/reopen, single instance, and graceful quit' | Set-Content $ResultPath
} catch {
    "FAIL: $_" | Set-Content $ResultPath
    throw
} finally {
    Remove-Item Env:\QT_QPA_PLATFORM -ErrorAction SilentlyContinue
    if ($settingsIsolated -and (!$app -or $app.HasExited)) {
        if (Test-Path 'HKCU:\Software\SoundCurrent\soundcurrent-eq') { RunRegistry ('delete "'+$key+'" /f') }
        if (Test-Path $backup) { RunRegistry ('import "'+$backup+'"'); Remove-Item $backup }
    }
    # Leave a failed desktop check running for inspection and graceful recovery.
}
