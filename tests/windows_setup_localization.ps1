# SPDX-License-Identifier: GPL-3.0-only
param([Parameter(Mandatory=$true)][string]$CatalogPath)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'packaging/windows/setup-localization.ps1')
$script:SCSetupCatalogPath = $CatalogPath
$data = Get-Content -LiteralPath $CatalogPath -Raw -Encoding UTF8 | ConvertFrom-Json
$count = 0
foreach ($pack in $data.languages.PSObject.Properties) {
    $expected = $pack.Value.PSObject.Properties['Audio driver setup'].Value
    $actual = Get-SCSetupText 'Audio driver setup' -Language $pack.Name
    if (!$expected -or $actual -cne $expected) { throw "Helper title lookup failed: $($pack.Name)" }
    if ($pack.Name -ne 'en' -and $actual -ceq 'Audio driver setup') { throw "Untranslated helper title: $($pack.Name)" }
    $count++
}
$available = @($data.languages.PSObject.Properties.Name)
foreach ($case in @(@('FR_ca','fr'),@('NN_no','nn'),@('zh-HK','zh-Hant'),@('zh-CN','zh-Hans'),@('pt-AO','en'),@('fr-Xxxx','en'),@('not-a-language','en'))) {
    if ((Resolve-SCSetupLanguage $case[0] $available) -ne $case[1]) { throw "Helper language resolution failed: $($case[0])" }
}
$external = 'Unknown external %1 / 音声'
if ((Get-SCSetupText $external -Language ar) -cne $external) { throw 'Unknown diagnostic text changed' }
$script:SCSetupLanguages = $null
$script:SCSetupCatalogPath = Join-Path ([IO.Path]::GetTempPath()) ('missing-' + [guid]::NewGuid().ToString('N'))
if ((Get-SCSetupText 'Audio driver setup' -Language fr) -cne 'Audio driver setup') { throw 'Missing package data fallback failed' }
$script:SCSetupLanguages = [pscustomobject]@{fr=[pscustomobject]@{'Audio driver setup'=42}}
if ((Get-SCSetupText 'Audio driver setup' -Language fr) -cne 'Audio driver setup') { throw 'Non-string translation fallback failed' }
$badData = Join-Path ([IO.Path]::GetTempPath()) ('bad-setup-data-' + [guid]::NewGuid().ToString('N') + '.json')
try {
    foreach ($content in @('{not json', '{"schema":2,"languages":{}}')) {
        [IO.File]::WriteAllText($badData,$content,[Text.UTF8Encoding]::new($false))
        $script:SCSetupLanguages = $null
        $script:SCSetupCatalogPath = $badData
        if ((Get-SCSetupText 'Audio driver setup' -Language fr) -cne 'Audio driver setup') { throw 'Invalid package data fallback failed' }
    }
} finally { Remove-Item -LiteralPath $badData -ErrorAction SilentlyContinue }
foreach ($file in Get-ChildItem (Join-Path $root 'packaging/windows') -Filter '*.ps1') {
    $tokens=$null;$errors=$null
    $null=[Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw "PowerShell parse failed: $($file.Name): $errors" }
}
Write-Output "PASS: $count helper title catalogs, locale selection, external fallback, missing data and PowerShell parsing; no driver/endpoint actions"
