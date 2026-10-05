# SPDX-License-Identifier: GPL-3.0-only
# Exact official Qt SDK archive; verify before extraction. Run on Windows.
param([string]$Destination = 'C:\Qt')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$uri = 'https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6120/qt6_6120_msvc2022_64/qt.qt6.6120.win64_msvc2022_64/6.12.0-0-202609280346qtbase-Windows-Windows_11_24H2-MSVC2022-Windows-Windows_11_24H2-X86_64.7z'
$sha = '219d1b86def3be418e31d704572bc4acc91b2ec373c65595c9fb12b42bf098dc'
$archive = Join-Path $env:TEMP 'soundcurrent-qtbase-6.12.0.7z'
Invoke-WebRequest -Uri $uri -OutFile $archive
if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha) { throw 'Qt archive checksum mismatch' }
 $prefix = Join-Path $Destination '6.12.0\msvc2022_64'
New-Item -ItemType Directory -Force -Path $prefix | Out-Null
& 7z x -y "-o$prefix" $archive | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Qt extraction failed' }
Write-Output (Join-Path $Destination '6.12.0\msvc2022_64')
