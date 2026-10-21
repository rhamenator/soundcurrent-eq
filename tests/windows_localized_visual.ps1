# SPDX-License-Identifier: GPL-3.0-only
# Use on an independent Windows VM clone. Captures Qt widgets; no native review.
param([Parameter(Mandatory=$true)][ValidateSet('EQ','Studio')][string]$Product,
 [Parameter(Mandatory=$true)][string]$ResultDirectory)
$ErrorActionPreference='Stop'
$name='soundcurrent-'+$Product.ToLower()
$exe=Join-Path $env:LOCALAPPDATA "Programs\SoundCurrent $Product\$name.exe"
if(!(Test-Path $exe)){throw 'Install the candidate first'}
if(Get-Process $name -ErrorAction SilentlyContinue){throw 'Quit the tested application first'}
New-Item -ItemType Directory -Force $ResultDirectory | Out-Null
$results=@()
foreach($locale in @('de','pl','ar')){
 $directory=Join-Path $ResultDirectory $locale
 New-Item -ItemType Directory -Force $directory | Out-Null
 $info=New-Object Diagnostics.ProcessStartInfo
 $info.FileName=$exe;$info.Arguments="--localization-ui-test --language $locale"
 $info.UseShellExecute=$false;$info.RedirectStandardError=$true;$info.RedirectStandardOutput=$true
 $info.EnvironmentVariables['QT_QPA_PLATFORM']='windows'
 $info.EnvironmentVariables['QT_FORCE_STDERR_LOGGING']='1'
 $info.EnvironmentVariables['SOUNDCURRENT_UI_SCREENSHOT_DIR']=$directory
 $p=New-Object Diagnostics.Process;$p.StartInfo=$info
 try {
  [void]$p.Start();$o=$p.StandardOutput.ReadToEndAsync();$e=$p.StandardError.ReadToEndAsync()
  if(!$p.WaitForExit(60000)){$p.Kill();throw 'Native Windows UI test timed out'}
  $p.Refresh();$log=$e.Result;$log | Set-Content -Encoding UTF8 (Join-Path $directory 'stderr.log')
  if($p.ExitCode -ne 0){throw "Native UI failed: $log"}
  if($log -notmatch "Localization UI: $locale -> $locale"){throw 'Expected catalog not loaded'}
  $images=@(Get-ChildItem $directory -Filter '*.png')
  if($images.Count -lt 3){throw 'Missing screenshots'}
  $results+=@{locale=$locale;passed=$true;screenshots=$images.Count}
 }finally{$p.Dispose()}
}
@{product=$Product;qtPlatform='windows';scope='Native Qt Windows widget rendering through SSH-launched test process; synthetic controls, no live audio';results=$results;nativeReviewed=$false;visualInspected=$false;interactiveDesktopQualification='pending'} | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $ResultDirectory 'report.json')
Write-Output "PASS: $Product native Windows localized screenshots captured"
