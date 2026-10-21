# SPDX-License-Identifier: GPL-3.0-only
# Observe an actual sign-in on a full independent Windows clone; reopen and quit the tested app.
param([switch]$Run,[Parameter(Mandatory=$true)][ValidateSet('EQ','Studio')][string]$Product,
 [Parameter(Mandatory=$true)][string]$ResultDirectory,
 [string]$RegistrationScope="Actual sign-in invocation; GUI checkbox activation is not established by this observer.")
$ErrorActionPreference='Stop'
if(!$Run){Write-Output 'Use -Run only for an authorized independent clone sign-in check.';exit 0}
$out=$ResultDirectory
try {
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$target=$Product
$name='soundcurrent-'+$target.ToLower()
$deadline=[DateTime]::UtcNow.AddSeconds(150)
do {$p=Get-Process $name -ErrorAction SilentlyContinue;if(!$p){Start-Sleep -Milliseconds 500}} while(!$p -and [DateTime]::UtcNow -lt $deadline)
if(!$p){throw 'No automatic startup process appeared'}
Start-Sleep -Seconds 3
$p.Refresh()
$session=[Diagnostics.Process]::GetCurrentProcess().SessionId
if($session -eq 0 -or $p.SessionId -ne $session){throw 'Observer is outside app desktop session'}
if(@(Get-Process soundcurrent-eq,soundcurrent-studio -ErrorAction SilentlyContinue).Count -ne 1){throw 'More than one app started'}
if($p.MainWindowHandle -ne 0){throw 'Startup displayed main window'}
$command=(Get-CimInstance Win32_Process -Filter "ProcessId=$($p.Id)").CommandLine
if($command -notmatch '\s--background(?:\s|$)'){throw 'Startup missing background argument'}
function Capture($suffix){
 $b=[Windows.Forms.Screen]::PrimaryScreen.Bounds;$bmp=New-Object Drawing.Bitmap($b.Width,$b.Height);$g=[Drawing.Graphics]::FromImage($bmp)
 $g.CopyFromScreen($b.Location,[Drawing.Point]::Empty,$b.Size);$bmp.Save((Join-Path $out "$target-$suffix.png"));$g.Dispose();$bmp.Dispose()
}
Capture 'background'
$exe=$p.Path;$id=$p.Id
$a=Start-Process $exe -PassThru
if(!$a.WaitForExit(15000)){throw 'Activation instance did not exit'}
$deadline=[DateTime]::UtcNow.AddSeconds(15)
do {$p.Refresh();if($p.MainWindowHandle -eq 0){Start-Sleep -Milliseconds 100}} while($p.MainWindowHandle -eq 0 -and [DateTime]::UtcNow -lt $deadline)
if($p.MainWindowHandle -eq 0){throw 'Activation did not reopen window'}
Capture 'reopened'
$a=Start-Process $exe -ArgumentList '--quit' -PassThru
if(!$a.WaitForExit(15000)){throw 'Quit command timed out'}
if(!$p.WaitForExit(15000)){throw 'App did not quit'}
@{product=$target;actualSignIn=$true;interactiveObserverSession=$session;appSession=$session;observedCommand=$command;exeSha256=(Get-FileHash $exe).Hash;backgroundWindowHidden=$true;exclusiveApp=$true;reopenedSameProcess=$true;quitExited=$true;nativeReviewed=$false;registrationScope=$RegistrationScope} | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $out "$target-desktop-observed.json")
'PASS' | Set-Content -Encoding UTF8 (Join-Path $out "$target-status.txt")
}catch { $_.Exception.Message | Set-Content -Encoding UTF8 (Join-Path $out 'observer-error.txt');exit 1 }
