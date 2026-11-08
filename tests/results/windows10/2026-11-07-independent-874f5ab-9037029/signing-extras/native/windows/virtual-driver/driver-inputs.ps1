# SPDX-License-Identifier: GPL-3.0-only
# Pure input/snapshot validation. No certificate, signing, trust or driver action.
function Get-VerifiedUnsignedDriverInputs {
 [CmdletBinding()]
 param([Parameter(Mandatory=$true)][string]$BuildDirectory)
 $manifest=Join-Path $BuildDirectory 'unsigned-driver-sha256.json'
 if((Get-Item -LiteralPath $manifest -ErrorAction Stop).Length -gt 1048576){throw 'Driver input manifest is too large'}
 $parsed=Get-Content -LiteralPath $manifest -Raw -Encoding UTF8 -ErrorAction Stop|ConvertFrom-Json -ErrorAction Stop
 $inputs=@($parsed)
 if($inputs.Count -ne 3){throw 'Expected exactly three driver input hashes'}
 $seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
 foreach($item in $inputs){
  if($item.File -isnot [string] -or $item.Hash -isnot [string] -or $item.Hash -notmatch '^[A-Fa-f0-9]{64}$'){throw 'Invalid driver input hash record'}
  if($item.File -notin 'soundcurrentvad.sys','soundcurrentvad.inf','soundcurrentvad.cat' -or !$seen.Add($item.File)){throw 'Unexpected or duplicate driver input file'}
 }
 # A closed whitelist plus three case-insensitively distinct names requires all
 # three files. Select-Object -Unique alone is case-sensitive, unlike NTFS paths.
 foreach($item in $inputs){
  $name=$item.File.ToLowerInvariant()
  if((Get-FileHash -LiteralPath (Join-Path "$BuildDirectory\unsigned-driver" $name) -Algorithm SHA256 -ErrorAction Stop).Hash -ne $item.Hash){throw 'Unsigned input checksum mismatch'}
  [pscustomobject]@{File=$name;Hash=$item.Hash.ToUpperInvariant()}
 }
}
function Copy-VerifiedUnsignedDriverInputs {
 [CmdletBinding()]
 param([Parameter(Mandatory=$true)][string]$BuildDirectory,
       [Parameter(Mandatory=$true)][string]$DestinationDirectory)
 $inputs=@(Get-VerifiedUnsignedDriverInputs -BuildDirectory $BuildDirectory)
 if(!(Test-Path -LiteralPath $DestinationDirectory -PathType Container) -or @(Get-ChildItem -LiteralPath $DestinationDirectory -Force -ErrorAction Stop).Count){throw 'Use a new empty signing package directory'}
 foreach($name in 'soundcurrentvad.inf','soundcurrentvad.sys'){
  $target=Join-Path $DestinationDirectory $name
  # Never overwrite an existing file. Verify the copied bytes, closing the
  # source-check/copy race before any signing command is invoked.
  [IO.File]::Copy((Join-Path "$BuildDirectory\unsigned-driver" $name),$target,$false)
  (Get-Item -LiteralPath $target -ErrorAction Stop).IsReadOnly=$false
  $expected=($inputs|Where-Object File -eq $name).Hash
  if((Get-FileHash -LiteralPath $target -Algorithm SHA256 -ErrorAction Stop).Hash -ne $expected){throw 'Copied driver input checksum mismatch; do not sign'}
 }
}
