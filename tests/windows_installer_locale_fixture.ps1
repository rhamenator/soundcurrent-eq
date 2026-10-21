# SPDX-License-Identifier: GPL-3.0-only
# Inert fixture: parse lifecycle script and run its pure data helper only.
param([Parameter(Mandatory=$true)][string]$CatalogPath,[Parameter(Mandatory=$true)][string]$LanguageMapPath)
$ErrorActionPreference='Stop'
$path=Join-Path $PSScriptRoot 'windows_installer_locale_lifecycle.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw ('Lifecycle PowerShell parse failure: '+$errors)}
$helper=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-LocaleExpectation'},$true)
if(!$helper){throw 'Missing pure locale expectation helper'}
. ([scriptblock]::Create($helper.Extent.Text))
$catalog=Get-Content -LiteralPath $CatalogPath -Raw -Encoding UTF8 | ConvertFrom-Json
$map=Get-Content -LiteralPath $LanguageMapPath -Raw -Encoding UTF8 | ConvertFrom-Json
$count=0
foreach($row in $map.languages){
 $expected=Get-LocaleExpectation $catalog $map $row.tag
 if($expected.tag -cne $row.tag -or $expected.id -ne $row.windowsLanguageId){throw 'Locale expectation identity mismatch'}
 foreach($key in @('SCShortcutUninstall','SCShortcutSetup','SCShortcutCableSettings')){
  if(!$expected.captions.$key -or $expected.captions.$key -cne $catalog.languages.PSObject.Properties[$row.tag].Value.cable.shortcutCaptions.$key){throw 'Shortcut caption expectation mismatch'}
 }
 $count++
}
if($count -ne 34){throw 'Lifecycle fixture needs all 34 locale identities'}
$rejected=$false
try{Get-LocaleExpectation $catalog $map 'unknown' | Out-Null}catch{$rejected=$true}
if(!$rejected){throw 'Unknown lifecycle locale was accepted'}
Write-Output 'PASS: lifecycle parser and pure expectation helper across 34 locales; no installation, registry, COM, audio or process mutations'
