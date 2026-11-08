# SPDX-License-Identifier: GPL-3.0-only
# Runs on PowerShell 5.1/7 without a certificate, WDK, admin or a real driver.
$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\native\windows\virtual-driver\driver-inputs.ps1"
$root=Join-Path ([IO.Path]::GetTempPath()) ('soundcurrent-sign-inputs-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory "$root\unsigned-driver"|Out-Null
try{
 foreach($name in 'soundcurrentvad.inf','soundcurrentvad.sys','soundcurrentvad.cat'){[IO.File]::WriteAllText((Join-Path "$root\unsigned-driver" $name),'synthetic '+$name)}
 $valid=@(Get-ChildItem "$root\unsigned-driver"|Get-FileHash|ForEach-Object{[pscustomobject]@{File=[IO.Path]::GetFileName($_.Path);Hash=$_.Hash}})
 function Write-Manifest($rows){ConvertTo-Json -InputObject @($rows)|Set-Content -Encoding UTF8 "$root\unsigned-driver-sha256.json"}
 function Reject($rows){Write-Manifest $rows;$denied=$false;try{$null=@(Get-VerifiedUnsignedDriverInputs $root)}catch{$denied=$true};if(!$denied){throw 'Malformed signing manifest accepted'}}
 Write-Manifest $valid
 if(@(Get-VerifiedUnsignedDriverInputs $root).Count -ne 3){throw 'Valid signing inputs rejected'}
 $inf=$valid|Where-Object File -eq 'soundcurrentvad.inf';$cat=$valid|Where-Object File -eq 'soundcurrentvad.cat'
 Reject @($inf,[pscustomobject]@{File='SOUNDCURRENTVAD.INF';Hash=$inf.Hash},$cat)
 Reject @($inf,$cat)
 Reject @($inf,$cat,[pscustomobject]@{File='..\soundcurrentvad.sys';Hash=$inf.Hash})
 Reject @($inf,$cat,[pscustomobject]@{File='soundcurrentvad.sys';Hash=('0'*64)})
 Reject @($inf,$cat,[pscustomobject]@{File='soundcurrentvad.sys';Hash='not-a-hash'})
 Reject @($inf,$cat,[pscustomobject]@{File=@('soundcurrentvad.sys');Hash=$inf.Hash})
 Write-Manifest $valid
 $package=New-Item -ItemType Directory "$root\snapshot"
 Copy-VerifiedUnsignedDriverInputs $root $package.FullName
 foreach($name in 'soundcurrentvad.inf','soundcurrentvad.sys'){
  if((Get-FileHash (Join-Path $package.FullName $name)).Hash -ne ($valid|Where-Object File -eq $name).Hash){throw 'Snapshot changed input bytes'}
 }
 $denied=$false;try{Copy-VerifiedUnsignedDriverInputs $root $package.FullName}catch{$denied=$true};if(!$denied){throw 'Signing snapshot overwrote existing files'}
 Write-Output 'PASS: exact manifest coverage, case-alias rejection, malformed hashes/paths/types and verified non-overwriting signing snapshot; no key/signing actions'
}finally{Remove-Item -LiteralPath $root -Recurse -Force}
