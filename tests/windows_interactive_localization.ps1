# SPDX-License-Identifier: GPL-3.0-only
# Run only in a signed-in desktop session on a full independent Windows clone.
param([switch]$Run,[Parameter(Mandatory=$true)][ValidateSet('EQ','Studio')][string]$Product,
 [Parameter(Mandatory=$true)][string]$InstallerPath,[Parameter(Mandatory=$true)][string]$ResultDirectory)
$ErrorActionPreference='Stop'
if(!$Run){Write-Output 'Use -Run only in an independent clone desktop session.';exit 0}
if([Diagnostics.Process]::GetCurrentProcess().SessionId -eq 0){throw 'An authenticated interactive desktop session is required'}
if(Get-Process ('soundcurrent-'+$Product.ToLower()) -ErrorAction SilentlyContinue){throw 'Quit the tested app first'}
$out=$ResultDirectory
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item (Join-Path $out "error.txt"),(Join-Path $out "status.txt"),(Join-Path $out "result.json") -ErrorAction SilentlyContinue
try {
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
Add-Type -TypeDefinition @'
using System; using System.Collections.Generic; using System.Runtime.InteropServices; using System.Text;
public class ScWindow {
 public delegate bool EnumProc(IntPtr h,IntPtr p);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb,IntPtr p);
 [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h,EnumProc cb,IntPtr p);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
 [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,StringBuilder b,int n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 public static IntPtr Top(int pid){IntPtr found=IntPtr.Zero; EnumWindows((h,p)=>{uint id;GetWindowThreadProcessId(h,out id);if(id==pid && IsWindowVisible(h)){found=h;return false;}return true;},IntPtr.Zero);return found;}
 public static IntPtr[] Children(IntPtr h){var a=new List<IntPtr>();EnumChildWindows(h,(w,p)=>{if(IsWindowVisible(w))a.Add(w);return true;},IntPtr.Zero);return a.ToArray();}
 public static string Text(IntPtr h){var b=new StringBuilder(2048);GetWindowText(h,b,b.Capacity);return b.ToString();}
}
'@
$installer=$InstallerPath
$p=Start-Process $installer -PassThru
$driverCleared=$false;$runCleared=$false
$pages=@();$deadline=[DateTime]::UtcNow.AddSeconds(120);$last='';$capture=0
while(!$p.HasExited -and [DateTime]::UtcNow -lt $deadline){
 $top=[ScWindow]::Top($p.Id)
 if($top -eq [IntPtr]::Zero){Start-Sleep -Milliseconds 200;$p.Refresh();continue}
 $children=@([ScWindow]::Children($top));$texts=@($children | ForEach-Object {[ScWindow]::Text($_)})
 $joined=$texts -join "`n"
 foreach($h in $children){$t=([ScWindow]::Text($h)).Replace("&","");if($t -match '^Install VB-CABLE if missing|^Run SoundCurrent') {[void][ScWindow]::SendMessage($h,0xF1,[IntPtr]::Zero,[IntPtr]::Zero);if([ScWindow]::SendMessage($h,0xF0,[IntPtr]::Zero,[IntPtr]::Zero) -ne [IntPtr]::Zero){throw 'Checkbox could not be cleared'};if($t -match '^Install VB-CABLE'){$driverCleared=$true}else{$runCleared=$true}}}
 $button=$children | Where-Object {([ScWindow]::Text($_)) -match '^&?Next\s*>?$|^I &?Agree$|^&?Install$|^&?Finish$'} | Select-Object -First 1
 if($button -and [ScWindow]::IsWindowEnabled($button)){
 if($joined -ne $last){
  $capture++;$bounds=[Windows.Forms.Screen]::PrimaryScreen.Bounds
  $bitmap=New-Object Drawing.Bitmap($bounds.Width,$bounds.Height);$g=[Drawing.Graphics]::FromImage($bitmap)
  $g.CopyFromScreen($bounds.Location,[Drawing.Point]::Empty,$bounds.Size);$bitmap.Save((Join-Path $out "page-$capture.png"));$g.Dispose();$bitmap.Dispose()
  $pages+=@{button=[ScWindow]::Text($button);texts=$texts};$last=$joined
  }
  [void][ScWindow]::PostMessage($top,0x111,[IntPtr]([ScWindow]::GetDlgCtrlID($button)),$button)
 }
 Start-Sleep -Milliseconds 1000;$p.Refresh()
}
if(!$p.HasExited){Stop-Process -Id $p.Id -Force;throw 'Interactive installer timed out'}
if($p.ExitCode -ne 0){throw "Installer exit $($p.ExitCode)"}
if($pages.Count -lt 5 -or !$driverCleared -or !$runCleared){throw 'Required pages or unchecked driver/launch controls were not observed'}
$allText=($pages | ForEach-Object {$_.texts -join "`n"}) -join "`n"
foreach($phrase in @("$([char]0x2014) donations are welcome","app$([char]0x2019)s uninstaller","$([char]0x2022) GPL-3.0-only")){if(!$allText.Contains($phrase)){throw ('Installer Unicode text missing: '+$phrase)}}
if(Get-Process ('soundcurrent-'+$Product.ToLower()) -ErrorAction SilentlyContinue){throw 'Finish unexpectedly launched the app'}
@{product=$Product;sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId;installerSha256=(Get-FileHash $installer).Hash;exitCode=$p.ExitCode;pages=$pages;driverInstallUnchecked=$driverCleared;finishRunUnchecked=$runCleared;unicodePunctuationVerified=$true;nativeReviewed=$false} | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $out 'result.json')
'PASS' | Set-Content -Encoding UTF8 (Join-Path $out 'status.txt')
}catch { $_.Exception.Message | Set-Content -Encoding UTF8 (Join-Path $out 'error.txt');exit 1 }
