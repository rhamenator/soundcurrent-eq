# SPDX-License-Identifier: GPL-3.0-only
# Run in an independent Windows test clone's signed-in desktop.
# Use Missing only after explicitly removing VB-CABLE in that clone.
# -InstallDriver offers the real driver install: approve UAC and the vendor
# dialog manually. The default check opts out without changing drivers.
param(
    [Parameter(Mandatory=$true)][string]$InstallerPath,
    [ValidateSet('Present','Missing')][string]$CableState = 'Present',
    [switch]$InstallDriver,
    [string]$ResultPath = "$env:TEMP\soundcurrent-installer-result.txt"
)
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class SetupUi {
 public delegate bool Callback(IntPtr w,IntPtr p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr w,int id);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr w,int m,IntPtr p,IntPtr l);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,int m,IntPtr p,IntPtr l);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr w);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr w,Callback c,IntPtr p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr w,StringBuilder s,int n);
 public static IntPtr[] Children(IntPtr parent) {
  var children=new List<IntPtr>();
  EnumChildWindows(parent,(w,p)=>{children.Add(w);return true;},IntPtr.Zero);
  return children.ToArray();
 }
 public static string Text(IntPtr w) {var s=new StringBuilder(2048);GetWindowText(w,s,s.Capacity);return s.ToString();}
}
'@
function Assert($value,$message) {if(-not $value){throw $message}}
function Click($handle) {[void][SetupUi]::PostMessage($handle,0xF5,0,0);Start-Sleep -Milliseconds 500}
function Next {
    [void][SetupUi]::PostMessage($script:window,0x111,1,[SetupUi]::GetDlgItem($script:window,1))
    Start-Sleep -Milliseconds 500
}
try {
    Assert (-not (Get-Process soundcurrent-eq -ErrorAction SilentlyContinue)) 'Quit the EQ before testing setup'
    $process=Start-Process $InstallerPath -PassThru
    $deadline=[DateTime]::UtcNow.AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 100
        $script:window=[SetupUi]::FindWindow('#32770','SoundCurrent EQ Setup')
    } while($script:window -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    Assert ($script:window -ne [IntPtr]::Zero) 'Setup window missing'
    Next # Welcome
    Next # GPL agreement
    Next # Installation folder
    $controls=@([SetupUi]::Children($script:window) | Where-Object {[SetupUi]::IsWindowVisible($_)})
    $texts=@($controls | ForEach-Object {[SetupUi]::Text($_)})
    Assert (($texts -join "`n") -match 'VB-CABLE website / donations') 'Vendor donation link missing'
    Assert (($texts -join "`n") -match 'VB-Audio licensing terms') 'Vendor licensing link missing'
    $choice=@($controls | Where-Object {[SetupUi]::Text($_) -eq 'Install the standard VB-CABLE driver'})
    Assert ($choice.Count -eq 1) 'Driver option missing'
    $checked=[SetupUi]::SendMessage($choice[0],0xF0,0,0).ToInt32()
    if($CableState -eq 'Present') {
        Assert (-not [SetupUi]::IsWindowEnabled($choice[0])) 'Existing driver was offered for reinstallation'
        Assert ($checked -eq 0) 'Existing driver option checked'
        Assert (($texts -join "`n") -match 'VB-CABLE is already installed') 'Existing driver not detected'
    } else {
        Assert ([SetupUi]::IsWindowEnabled($choice[0])) 'Missing driver option disabled'
        Assert ($checked -eq 1) 'Missing driver was not offered by default'
        if(-not $InstallDriver) {Click $choice[0]}
    }
    Next # Install app
    $deadline=[DateTime]::UtcNow.AddSeconds($(if($InstallDriver){180}else{30}))
    do {Start-Sleep -Milliseconds 100} while([SetupUi]::Text([SetupUi]::GetDlgItem($script:window,1)) -ne '&Finish' -and [DateTime]::UtcNow -lt $deadline)
    Assert ([SetupUi]::Text([SetupUi]::GetDlgItem($script:window,1)) -eq '&Finish') 'Installation did not finish'
    foreach($control in [SetupUi]::Children($script:window)) {
        if([SetupUi]::Text($control) -match 'Run.*SoundCurrent') {
            [void][SetupUi]::SendMessage($control,0xF1,0,0)
        }
    }
    Next
    Assert ($process.WaitForExit(5000)) 'Setup did not exit'
    $folder="$env:LOCALAPPDATA\Programs\SoundCurrent EQ"
    Assert (Test-Path "$folder\audio-setup.ps1") 'Retry helper missing'
    Assert (Test-Path "$folder\VBCABLE_Driver_Pack45.zip") 'Vendor archive missing'
    Assert (Test-Path "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\SoundCurrent EQ\Install VB-CABLE.lnk") 'Retry shortcut missing'
    "PASS: $CableState driver detection, default option, vendor notices, app installation and retry shortcut" | Set-Content $ResultPath
} catch {
    "FAIL: $_" | Set-Content $ResultPath
    throw
}
