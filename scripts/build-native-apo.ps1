# SPDX-License-Identifier: GPL-3.0-only
param([string]$BuildDirectory='build-native-apo')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
& cmake -S "$root/native/windows" -B "$root/$BuildDirectory" -G 'Visual Studio 17 2022' -A x64 -T v143
if($LASTEXITCODE -ne 0){throw 'Native APO configure failed'}
& cmake --build "$root/$BuildDirectory" --config Release --parallel 4
if($LASTEXITCODE -ne 0){throw 'Native APO build failed'}
& ctest --test-dir "$root/$BuildDirectory" -C Release --output-on-failure
if($LASTEXITCODE -ne 0){throw 'Native APO test failed'}
Write-Output 'Native APO harness passed. Experimental DLL is unregistered; no device or default-output changes were made.'
