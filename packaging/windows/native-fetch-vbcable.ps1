# SPDX-License-Identifier: GPL-3.0-only
# Downloads VB-Audio's official VB-CABLE package only when the user ticked "Also remove VB-CABLE" in the
# installer, and keeps it only if its SHA-256 matches the pinned release. Exit 0 = ready, 1 = not available.
param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$url = 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip'
$sha256 = 'b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb'
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $directory = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Force $directory | Out-Null
    $temporary = "$Destination.download"
    Invoke-WebRequest -Uri $url -OutFile $temporary -UseBasicParsing -TimeoutSec 60
    if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) {
        Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
        exit 1
    }
    Move-Item -LiteralPath $temporary -Destination $Destination -Force
    exit 0
} catch {
    Remove-Item -LiteralPath "$Destination.download" -Force -ErrorAction SilentlyContinue
    exit 1
}
