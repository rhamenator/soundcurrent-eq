# SPDX-License-Identifier: GPL-3.0-only
param([Parameter(Mandatory=$true)][string]$CatalogPath)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'packaging/windows/setup-localization.ps1')
$script:SCSetupCatalogPath = $CatalogPath
$data = Get-Content -LiteralPath $CatalogPath -Raw -Encoding UTF8 | ConvertFrom-Json
$required = Get-Content -LiteralPath (Join-Path $root 'data/localization/setup-sources.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$count = 0
foreach ($pack in $data.languages.PSObject.Properties) {
    foreach ($source in $required) {
        $expected = $pack.Value.PSObject.Properties[$source].Value
        $actual = Get-SCSetupText $source -Language $pack.Name
        if (!$expected -or $actual -cne $expected) { throw "Helper lookup failed: $($pack.Name): $source" }
        if ($pack.Name -ne 'en' -and $actual -ceq $source) { throw "Untranslated helper text: $($pack.Name): $source" }
        $count++
    }
}
$available = @($data.languages.PSObject.Properties.Name)
foreach ($case in @(@('FR_ca','fr'),@('NN_no','nn'),@('zh-HK','zh-Hant'),@('zh-CN','zh-Hans'),@('pt-AO','en'),@('fr-Xxxx','en'),@('not-a-language','en'))) {
    if ((Resolve-SCSetupLanguage $case[0] $available) -ne $case[1]) { throw "Helper language resolution failed: $($case[0])" }
}
if ((Get-SCSetupText 'audio driver setup' -Language fr) -cne 'audio driver setup') { throw 'Case-folding altered unknown source text' }
$external = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('VW5rbm93biBleHRlcm5hbCAlMSAvIOmfs+WjsA=='))
if ((Get-SCSetupText $external -Language ar) -cne $external) { throw 'Unknown diagnostic text changed' }
$script:SCSetupLanguages = $null
$script:SCSetupCatalogPath = Join-Path ([IO.Path]::GetTempPath()) ('missing-' + [guid]::NewGuid().ToString('N'))
if ((Get-SCSetupText 'Audio driver setup' -Language fr) -cne 'Audio driver setup') { throw 'Missing package data fallback failed' }
$script:SCSetupLanguages = [pscustomobject]@{fr=[pscustomobject]@{'Audio driver setup'=42}}
if ((Get-SCSetupText 'Audio driver setup' -Language fr) -cne 'Audio driver setup') { throw 'Non-string translation fallback failed' }
# Formatting fixtures are isolated data, not production driver operations.
$script:SCSetupLanguages = [pscustomobject]@{fr=[pscustomobject]@{
    '%1 / %2 / %10'='%10 / %2 / %1'; '%1 + %1'='%1 + %1';
    '%1 / %2'='%1'; 'Code %1'='Code %L1'
}}
$values=@($external,'%1 literal / $1 / C:\Users\Name', '3','4','5','6','7','8','9','TEN')
$formatted=Format-SCSetupText '%1 / %2 / %10' -Values $values -Language fr
if ($formatted -cne ('TEN / ' + $values[1] + ' / ' + $external)) { throw 'Reordered/multidigit setup formatting altered data' }
if ((Format-SCSetupText '%1 + %1' -Values @($external) -Language fr) -cne ($external + ' + ' + $external)) { throw 'Repeated placeholder formatting failed' }
if ((Format-SCSetupText '%1 / %2' -Values @('A','B') -Language fr) -cne 'A / B') { throw 'Corrupt placeholder fallback failed' }
if ((Format-SCSetupText 'Code %1' -Values @('0x80004005') -Language fr) -cne 'Code 0x80004005') { throw 'Localized-number placeholder corruption lost error code' }
foreach ($invalid in @('%n tracks','%L1 dB','%100','%2')) {
    $rejected=$false
    try { $null=Format-SCSetupText $invalid -Values @('A') -Language fr } catch { $rejected=$true }
    if (!$rejected) { throw "Invalid/missing template value accepted: $invalid" }
}
$badData = Join-Path ([IO.Path]::GetTempPath()) ('bad-setup-data-' + [guid]::NewGuid().ToString('N') + '.json')
try {
    foreach ($content in @('{not json', '{"schema":2,"languages":{}}')) {
        [IO.File]::WriteAllText($badData,$content,[Text.UTF8Encoding]::new($false))
        $script:SCSetupLanguages = $null
        $script:SCSetupCatalogPath = $badData
        if ((Get-SCSetupText 'Audio driver setup' -Language fr) -cne 'Audio driver setup') { throw 'Invalid package data fallback failed' }
    }
} finally { Remove-Item -LiteralPath $badData -ErrorAction SilentlyContinue }
$literalLookups = @()
foreach ($file in Get-ChildItem (Join-Path $root 'packaging/windows') -Filter '*.ps1') {
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw "PowerShell parse failed: $($file.Name): $errors" }
    if ($file.Name -in @('cable-setup.ps1','native-audio-setup.ps1')) {
        $parameters=@($ast.ParamBlock.Parameters | ForEach-Object {$_.Name.VariablePath.UserPath})
        if ($parameters -notcontains 'Language') { throw "Helper language argument missing: $($file.Name)" }
        foreach ($command in $ast.FindAll({param($node)
            $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -in @('Get-SCSetupText','Format-SCSetupText')
        },$true)) {
            if ($command.CommandElements[1] -isnot [Management.Automation.Language.StringConstantExpressionAst]) {
                throw "Dynamic helper source needs explicit coverage review: $($file.Name)"
            }
            $literalLookups += $command.CommandElements[1].Value
        }
    }
}
$lookupDifference = Compare-Object @($required | Sort-Object -Unique) @($literalLookups | Sort-Object -Unique)
if ($lookupDifference) { throw "Declared helper source inventory differs from PowerShell AST: $lookupDifference" }
Write-Output "PASS: $count required helper text lookups, locale selection, external fallback, missing data and PowerShell parsing; no driver/endpoint actions"
