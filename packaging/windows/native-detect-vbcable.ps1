# SPDX-License-Identifier: GPL-3.0-only
# Exit 0 when the primary VB-CABLE driver is installed, 10 when it is not, 20 if it cannot be checked.
# Matches only the VB-CABLE hardware ID, never other VB-Audio products.
$ErrorActionPreference = 'Stop'
try {
    $found = @(Get-CimInstance Win32_PnPEntity | Where-Object {
        $_.HardwareID -contains 'VBAudioVACWDM' -or $_.HardwareID -contains 'ROOT\VBAudioVACWDM'
    }).Count -gt 0
} catch { exit 20 }
if ($found) { exit 0 }
exit 10
