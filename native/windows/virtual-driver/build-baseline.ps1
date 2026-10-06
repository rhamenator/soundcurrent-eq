# SPDX-License-Identifier: GPL-3.0-only
# Build only: never installs a driver, changes trust, or enables test signing.
param([string]$Configuration = "Release")
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$root = Join-Path $PSScriptRoot "vendor"
$tools = Join-Path $root ".tools"
New-Item -ItemType Directory -Force $tools | Out-Null
$nuget = Join-Path $tools "nuget.exe"
if (!(Test-Path $nuget)) {
    Invoke-WebRequest "https://dist.nuget.org/win-x86-commandline/v6.14.0/nuget.exe" -OutFile $nuget
}
$signature = Get-AuthenticodeSignature $nuget
if ($signature.Status -ne "Valid" -or $signature.SignerCertificate.Subject -notmatch "O=Microsoft Corporation") {
    throw "NuGet executable must have a valid Microsoft signature; restore was not executed."
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\amd64\MSBuild.exe" | Select-Object -First 1
if (!$msbuild) { throw "Install Visual Studio C++ build tools before building the driver." }
& $nuget restore (Join-Path $root "packages.config") -PackagesDirectory (Join-Path $root "packages") -Source "https://api.nuget.org/v3/index.json" -NonInteractive
if ($LASTEXITCODE -ne 0) { throw "SDK/WDK restore failed: $LASTEXITCODE" }
$stamp = Get-ChildItem (Join-Path $root "packages\Microsoft.Windows.WDK.x64.10.0.28000.2526") -Recurse -Filter stampinf.exe | Where-Object { $_.Directory.Name -eq "x64" } | Select-Object -First 1
if (!$stamp) { throw "WDK stampinf tool missing" }
$env:PATH = $stamp.Directory.FullName + ";" + $env:PATH
& $msbuild (Join-Path $root "sysvad\EndpointsCommon\EndpointsCommon.vcxproj") /m /p:Platform=x64 "/p:Configuration=$Configuration" /p:WindowsTargetPlatformVersion=10.0.28000.0 /p:SignMode=Off
if ($LASTEXITCODE -ne 0) { throw "Common driver library build failed: $LASTEXITCODE" }
& $msbuild (Join-Path $root "sysvad\TabletAudioSample\TabletAudioSample.vcxproj") /m /p:Platform=x64 "/p:Configuration=$Configuration" /p:WindowsTargetPlatformVersion=10.0.28000.0 /p:SignMode=Off
if ($LASTEXITCODE -ne 0) { throw "Driver baseline build failed: $LASTEXITCODE" }

$package = Join-Path $root "driver-package"
New-Item -ItemType Directory -Force $package | Out-Null
Copy-Item (Join-Path $root "sysvad\TabletAudioSample\x64\$Configuration\soundcurrentvad.sys"), (Join-Path $root "sysvad\TabletAudioSample\x64\$Configuration\soundcurrentvad.inf") $package
$inf2cat = Join-Path $root "packages\Microsoft.Windows.WDK.x64.10.0.28000.2526\c\bin\10.0.28000.0\x86\Inf2Cat.exe"
& $inf2cat "/driver:$package" /os:10_NI_X64,10_GE_X64,10_25H2_X64 /uselocaltime
if ($LASTEXITCODE -ne 0) { throw "Driver signability validation/catalog generation failed" }
Write-Output "Driver package built and validated; catalog is unsigned. Do not distribute as an installable release."
