# Independent clone test media only; no autorun.
param([switch]$Run,[ValidateSet('EQ','Studio')][string]$Product='EQ')
$ErrorActionPreference='Stop'
if(!$Run){Write-Output 'Use -Run only on soundcurrent-localization-win11-qa. No tests executed.';exit 0}
if($env:OS -ne 'Windows_NT'){throw 'Windows is required'}
$manifest=Get-Content (Join-Path $PSScriptRoot 'media-sha256.json') -Raw -Encoding UTF8 | ConvertFrom-Json
foreach($entry in $manifest.PSObject.Properties){
 if($entry.Name -match '[\\/]'){throw 'Invalid media filename'}
 if((Get-FileHash (Join-Path $PSScriptRoot $entry.Name) -Algorithm SHA256).Hash -ine $entry.Value){throw "Media checksum mismatch: $($entry.Name)"}
}
$prefix=if($Product -eq 'EQ'){'eq'}else{'studio'}
$name='soundcurrent-'+$Product.ToLower()
$result=Join-Path $env:LOCALAPPDATA ('SoundCurrentLocalizationQA\'+$Product+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $result -Force | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'package-identities.json') $result
Start-Transcript -Path (Join-Path $result 'console.log') | Out-Null
try {
 & (Join-Path $PSScriptRoot 'windows_installer_locale_fixture.ps1') -CatalogPath (Join-Path $PSScriptRoot ($name+'-captions.json')) -LanguageMapPath (Join-Path $PSScriptRoot 'installer-language-map.json')
 & (Join-Path $PSScriptRoot 'windows_installer_locale_lifecycle.ps1') -Run -Product $Product -InstallerPath (Join-Path $PSScriptRoot ($prefix+'-current.exe')) -InitialInstallerPath (Join-Path $PSScriptRoot ($prefix+'-old.exe')) -CatalogPath (Join-Path $PSScriptRoot ($name+'-captions.json')) -LanguageMapPath (Join-Path $PSScriptRoot 'installer-language-map.json') -ResultDirectory $result
 Write-Output ('Evidence: '+$result)
 Write-Output 'Qualification applies only to recorded installer hashes. No current-head, native-driver, chooser, or linguistic verification implied.'
}finally{Stop-Transcript | Out-Null}
