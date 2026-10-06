# SPDX-License-Identifier: GPL-3.0-only
# Opt-in guest coordinator. Host removes USB after ready, re-adds after fallback.
# Run only on an independent clone with the documented two-output fixture.
param([switch]$Run,[string]$Application,[string]$StatusFile)
if(!$Run){Write-Output 'Use -Run only on an independent Windows test clone.';exit 0}
$ErrorActionPreference='Stop'
if(!$Application -or !$StatusFile -or $StatusFile.Contains('"')){throw 'Supply application and status file paths'}
$Application=(Resolve-Path -LiteralPath $Application).Path
$env:PATH=(Split-Path $Application -Parent)+';'+$env:PATH
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts'
Remove-Item -LiteralPath $StatusFile -ErrorAction SilentlyContinue
$info=New-Object System.Diagnostics.ProcessStartInfo
$info.FileName=$Application
$info.Arguments='--windows-live-hotplug-test "'+$StatusFile+'"'
$info.UseShellExecute=$false;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
$process=New-Object System.Diagnostics.Process;$process.StartInfo=$info
try {
 if(!$process.Start()){throw 'Native launch failed'}
 $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
 if(!$process.WaitForExit(70000)){$process.Kill();$process.WaitForExit();throw 'Hotplug test timed out'}
 Write-Output $stdout.Result;Write-Output $stderr.Result
 $state=Get-Content -LiteralPath $StatusFile -Raw
 $code=$process.ExitCode
 if($null -eq $code -or $code -ne 0 -or $state.Trim() -ne 'reconnected'){throw "Hotplug exit/state mismatch: $code / $state"}
 Write-Output 'PASS: hotplug phases completed and native exit code is 0'
} finally {$process.Dispose()}
