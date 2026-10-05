# SPDX-License-Identifier: GPL-3.0-only
# Run in an isolated, signed-in Windows test desktop with speakers as default.
param([string]$InstallerPath, [string]$ResultPath = "$env:TEMP\soundcurrent-ui-result.txt")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class EqUi {
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr w,int id);
 [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr w,uint command);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr w,int m,IntPtr p,IntPtr l);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr w);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr w,StringBuilder b,int count);
}
'@
function Assert($Condition, $Message) { if (-not $Condition) { throw $Message } }
function Child($Id) { [EqUi]::GetDlgItem($script:window,$Id) }
function Send($Hwnd,$Message,$Wparam,$Lparam) { [EqUi]::SendMessage($Hwnd,$Message,[IntPtr]([long]$Wparam),[IntPtr]([long]$Lparam)) }
function Text($Hwnd) {
    $buffer = New-Object System.Text.StringBuilder 512
    [void][EqUi]::GetWindowText($Hwnd,$buffer,512)
    $buffer.ToString()
}
function Labels {
    for ($child=[EqUi]::GetWindow($script:window,5); $child -ne [IntPtr]::Zero; $child=[EqUi]::GetWindow($child,2)) { Text $child }
}
function Preset($Index) {
    [void](Send (Child 100) 0x14E $Index 0)
    [void](Send $script:window 0x111 0x10064 0)
}
function Slider($Id,$Value) {
    $child=Child $Id
    [void](Send $child 0x405 1 $Value)
    [void](Send $script:window $(if($Id -ge 200){0x115}else{0x114}) 5 $child)
    [void](Send $script:window $(if($Id -ge 200){0x115}else{0x114}) 8 $child)
}
$settings="$env:APPDATA\SoundCurrent EQ\settings.ini"
$original=$null
$hadSettings=Test-Path $settings
$app=$null
try {
    Assert (-not (Get-Process soundcurrent-eq -ErrorAction SilentlyContinue)) 'Quit the app before this test'
    if ($hadSettings) { $original=[IO.File]::ReadAllBytes($settings); Remove-Item $settings }
    if ($InstallerPath) {
        $installer=Start-Process $InstallerPath -ArgumentList '/S' -Wait -PassThru
        Assert ($installer.ExitCode -eq 0) 'Installer failed'
    }
    $exe="$env:LOCALAPPDATA\Programs\SoundCurrent EQ\soundcurrent-eq.exe"
    Assert (Test-Path $exe) 'Installed executable missing'
    Assert (Test-Path "$env:USERPROFILE\Desktop\SoundCurrent EQ.lnk") 'Desktop shortcut missing'
    Assert (Test-Path "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\SoundCurrent EQ\SoundCurrent EQ.lnk") 'Start menu shortcut missing'
    $app=Start-Process $exe -PassThru
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    do { Start-Sleep -Milliseconds 100; $script:window=[EqUi]::FindWindow('SoundCurrentEQWindow','SoundCurrent EQ') } while ($script:window -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    Assert ($script:window -ne [IntPtr]::Zero) 'App window missing'
    Assert ((Send (Child 100) 0x147 0 0).ToInt32() -eq 0) 'Flat is not the default'
    Assert ((Send (Child 100) 0x146 0 0).ToInt32() -eq 35) 'Preset list count differs'
    for($i=1;$i -lt 34;$i++) {
        Preset $i
        $changed=0
        for($b=200;$b -lt 215;$b++) { if((Send (Child $b) 0x400 0 0).ToInt32() -ne 0) {$changed++} }
        Assert ($changed -gt 0) "Preset $i had no effect"
    }
    Preset 0
    Slider 200 -60
    Assert ((Get-Content $settings) -contains 'band0=60') 'Upward band movement did not increase gain'
    [void](Send $script:window 0x111 105 0)
    Assert ((Send (Child 200) 0x400 0 0).ToInt32() -eq 0) 'Undo did not restore Flat band'
    Slider 106 -5
    Assert ((Labels) -contains 'Post gain  -0.5 dB') 'Negative post-gain label is incorrect'
    [void](Send $script:window 0x111 105 0)
    Assert ((Labels) -contains 'Post gain  +0.0 dB') 'Gain undo failed'
    [void](Send (Child 104) 0xF5 0 0)
    Assert (-not [EqUi]::IsWindowEnabled((Child 200))) 'Lock left bands editable'
    Assert (-not [EqUi]::IsWindowEnabled((Child 106))) 'Lock left gain editable'
    Assert (-not [EqUi]::IsWindowEnabled((Child 100))) 'Lock left presets editable'
    [void](Send (Child 104) 0xF5 0 0)
    [void](Send (Child 103) 0xF5 0 0)
    Assert ((Text (Child 103)) -eq 'Equalizer off') 'Bypass label did not update'
    [void](Send (Child 103) 0xF5 0 0)
    Assert ((Text (Child 103)) -eq 'Equalizer on') 'Enabled label did not update'
    [void](Send (Child 200) 0x20A 0x780000 0)
    Assert ((Send (Child 200) 0x400 0 0).ToInt32() -eq 0) 'Mouse wheel changed band gain'
    [void](Send $script:window 0x10 0 0)
    Start-Sleep -Milliseconds 300
    Assert (-not $app.HasExited) 'Closing the window unloaded the app'
    Assert (-not [EqUi]::IsWindowVisible($script:window)) 'Window did not hide'
    $second=Start-Process $exe -PassThru
    Assert ($second.WaitForExit(5000)) 'Second instance did not exit'
    Assert ([EqUi]::IsWindowVisible($script:window)) 'Relaunch did not restore window'
    [void](Send (Child 100) 0x14F 1 0)
    Assert ((Send (Child 100) 0x157 0 0).ToInt32() -ne 0) 'Scrollable preset popup did not open'
    [void](Send (Child 100) 0x14F 0 0)
    [void](Send $script:window 0x111 110 0)
    Assert ($app.WaitForExit(5000)) 'Quit did not unload the app; use speakers as Windows default for this test'
    'PASS: installer shortcuts, all presets, slider direction, gain labels, lock, undo, bypass, wheel protection, background close/reopen, dropdown, and quit' | Set-Content $ResultPath
} catch {
    "FAIL: $_" | Set-Content $ResultPath
    throw
} finally {
    if ($app -and -not $app.HasExited) {
        # Preserve the running app on failure so its state can be inspected.
    } elseif ($hadSettings) { [IO.File]::WriteAllBytes($settings,$original) }
    elseif (Test-Path $settings) { Remove-Item $settings }
}
