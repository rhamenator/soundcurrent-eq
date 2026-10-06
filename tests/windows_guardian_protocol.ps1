$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$exe=Join-Path $env:USERPROFILE 'soundcurrent-route-guardian.exe'
$start=New-Object System.Diagnostics.ProcessStartInfo
$start.FileName=$exe
$start.Arguments='0 nonexistent-owned nonexistent-fallback original-0 original-1 original-2 1 0'
$start.UseShellExecute=$false
$start.RedirectStandardInput=$true
$start.RedirectStandardOutput=$true
$p=New-Object System.Diagnostics.Process
$p.StartInfo=$start
if(!$p.Start()){throw 'Guardian did not start'}
if($p.StandardOutput.ReadLine() -ne 'ready'){throw 'Guardian handshake missing'}
if($p.HasExited){throw 'Guardian did not wait for its owner'}
$p.StandardInput.Close()
if(!$p.WaitForExit(10000)){ $p.Kill(); throw 'Guardian did not observe EOF' }
Write-Output ('Guardian EOF protocol exit='+$p.ExitCode)
# No selected role points to the fake owned endpoint: no route should be changed.
if($p.ExitCode -ne 0){throw 'Conditional restoration check failed'}
& $exe
if($LASTEXITCODE -ne 2){throw 'Malformed helper invocation was accepted'}
Write-Output 'Guardian handshake, EOF and malformed invocation checks passed'
