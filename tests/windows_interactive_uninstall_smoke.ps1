# SPDX-License-Identifier: GPL-3.0-only
# Destructive fixture: full independent Windows VM clone only.
param([switch]$Run)
if (!$Run) { Write-Output 'Use -Run only on a full independent Windows test clone.'; exit 0 }
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$log="$env:USERPROFILE\sc-interactive-uninstall-result.txt"
Add-Type -TypeDefinition @'
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public static class ScUninstallUi {
 public delegate bool CB(IntPtr w,IntPtr p);
 [StructLayout(LayoutKind.Sequential)] public struct Rect {public int left,top,right,bottom;}
 [DllImport("user32.dll")] public static extern bool EnumWindows(CB c,IntPtr p);
 [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr w,CB c,IntPtr p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr w,StringBuilder s,int n);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr w);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,int m,IntPtr p,IntPtr l);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra);
 public static string Text(IntPtr w){var b=new StringBuilder(4096);GetWindowText(w,b,b.Capacity);return b.ToString();}
 public static IntPtr[] Windows(){var l=new List<IntPtr>();EnumWindows((w,p)=>{if(IsWindowVisible(w))l.Add(w);return true;},IntPtr.Zero);return l.ToArray();}
 public static IntPtr[] Children(IntPtr w){var l=new List<IntPtr>();EnumChildWindows(w,(h,p)=>{if(IsWindowVisible(h))l.Add(h);return true;},IntPtr.Zero);return l.ToArray();}
}
'@
function Present { [bool](Get-CimInstance Win32_PnPEntity -Filter "PNPClass='MEDIA'" | Where-Object {$_.Name -eq 'VB-Audio Virtual Cable'}) }
function InstallApp($title,$version){
 $p=Start-Process "$env:USERPROFILE\SoundCurrent-$title-$version-windows-x64-VBCABLE-preview.exe" -ArgumentList '/S' -PassThru;$null=$p.Handle
 if(!$p.WaitForExit(180000) -or $p.ExitCode -ne 0){throw 'Silent install failed'}
}
function UninstallApp($title,$product,[bool]$RemoveCable){
 $reg="HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$product"
 $dir=Split-Path (Get-ItemProperty $reg).DisplayIcon -Parent
 Start-Process "$dir\uninstall.exe"|Out-Null
 $deadline=[DateTime]::UtcNow.AddSeconds(150);$vendorClicked=$false;$confirmation=$false
 while([DateTime]::UtcNow -lt $deadline){
  foreach($w in [ScUninstallUi]::Windows()){
   $name=[ScUninstallUi]::Text($w)
   if($name -eq 'Remove VB-CABLE?'){
    foreach($c in [ScUninstallUi]::Children($w)){
     $text=[ScUninstallUi]::Text($c)
     if(($RemoveCable -and $text -eq '&Yes') -or (!$RemoveCable -and $text -eq '&No')){
      [void][ScUninstallUi]::PostMessage($c,0xF5,[IntPtr]::Zero,[IntPtr]::Zero);$confirmation=$true
     }
    }
   } elseif($name -like 'VB-Audio Virtual Cable Driver Installation*' -and !$vendorClicked){
    if(!$RemoveCable){throw 'Unexpected driver mutation'}
    $r=New-Object ScUninstallUi+Rect;[void][ScUninstallUi]::GetWindowRect($w,[ref]$r)
    [void][ScUninstallUi]::SetForegroundWindow($w);[void][ScUninstallUi]::SetCursorPos($r.right-170,$r.bottom-52)
    Start-Sleep -Milliseconds 200
    [ScUninstallUi]::mouse_event(2,0,0,0,[UIntPtr]::Zero);[ScUninstallUi]::mouse_event(4,0,0,0,[UIntPtr]::Zero);$vendorClicked=$true
    Add-Content $log 'Clicked official Remove Driver in independent clone'
   } elseif($name -in @('VBCABLE Installation','VBCABLE Uninstallation')){
    foreach($c in [ScUninstallUi]::Children($w)){if([ScUninstallUi]::Text($c) -eq 'OK'){[void][ScUninstallUi]::PostMessage($c,0xF5,[IntPtr]::Zero,[IntPtr]::Zero)}}
   } elseif($name -like "SoundCurrent $title*" -and $name -match 'Uninstall'){
    foreach($c in [ScUninstallUi]::Children($w)){
     $text=[ScUninstallUi]::Text($c)
     if([ScUninstallUi]::IsWindowEnabled($c) -and $text -in @('&Uninstall','Uninstall','&Next >','Next >','&Finish','Finish','&Close','Close')){
      [void][ScUninstallUi]::PostMessage($c,0xF5,[IntPtr]::Zero,[IntPtr]::Zero)
     }
    }
   }
  }
  if(!(Test-Path $reg)){
   Start-Sleep -Seconds 2
   if(Test-Path "$dir\soundcurrent-$($title.ToLower()).exe"){throw 'Uninstall retained app executable'}
   if($RemoveCable -and (!$confirmation -or !$vendorClicked -or (Present))){throw 'Cable removal incomplete'}
   if(!$RemoveCable -and !(Present)){throw 'Shared cable incorrectly removed'}
   Add-Content $log "PASS: $title interactive uninstall; remove cable=$RemoveCable"
   return
  }
  Start-Sleep -Milliseconds 250
 }
 throw "Interactive uninstall timed out: $title"
}
try {
 Set-Content $log 'Independent clone: full NSIS interactive uninstall tests'
 InstallApp EQ '0.7.6';InstallApp Studio '0.8.5'
 UninstallApp EQ SoundCurrentEQ $false # Other app retains shared driver.
 UninstallApp Studio SoundCurrentStudio $false # Last app explicitly declines driver removal.
 InstallApp Studio '0.8.5'
 UninstallApp Studio SoundCurrentStudio $true # Last app accepts official signed remover.
 Add-Content $log 'INTERACTIVE UNINSTALL COMPLETED; REBOOT REQUIRED'
}catch{$_|Out-String|Add-Content $log;exit 1}
