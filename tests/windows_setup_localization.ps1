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
# Exercise the real declared dynamic templates in every installed catalog.
foreach ($pack in $data.languages.PSObject.Properties) {
    foreach ($source in @($required | Where-Object {$_ -match '%1'})) {
        foreach ($value in @('-2147024891','0x80004005','%1 / $1 / C:\Windows')) {
            $expected=$pack.Value.PSObject.Properties[$source].Value.Replace('%1',$value)
            $actual=Format-SCSetupText $source -Values @($value) -Language $pack.Name
            if ($actual -cne $expected) { throw "Owned setup code formatting failed: $($pack.Name)" }
        }
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
        # Run only the trusted fallback function definition, never driver script actions.
        $fallback=$ast.Find({param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Format-SCSetupText'
        },$true)
        if (!$fallback) { throw "Missing English template fallback: $($file.Name)" }
        $fallbackResult = & {
            . ([scriptblock]::Create($fallback.Extent.Text))
            Format-SCSetupText 'Code %1' -Values @('%1 / $1 / C:\Windows')
        }
        if ($fallbackResult -cne 'Code %1 / $1 / C:\Windows') { throw "Fallback altered literal code data: $($file.Name)" }
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

# New raw helper prose must not silently bypass declared translation coverage.
$inventoryScript=Join-Path $root 'scripts/windows_setup_inventory.ps1'
$inventoryDirectory=Join-Path ([IO.Path]::GetTempPath()) ('setup-prose-'+[guid]::NewGuid().ToString('N'))
try {
    $null=New-Item -ItemType Directory -Path $inventoryDirectory
    foreach ($name in @('cable-setup.ps1','native-audio-setup.ps1')) {
        Copy-Item -LiteralPath (Join-Path $root ('packaging/windows/'+$name)) -Destination $inventoryDirectory
    }
    $audit=(& $inventoryScript -SourceDirectory $inventoryDirectory) | ConvertFrom-Json
    $backlog=Get-Content -LiteralPath (Join-Path $root 'data/localization/setup-prose-backlog.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $known=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($row in $backlog.candidates) { $null=$known.Add(($row.file+"`0"+$row.kind+"`0"+$row.literal)) }
    foreach ($row in $audit.candidates) {
        if (!$known.Contains(($row.file+"`0"+$row.kind+"`0"+$row.literal))) {
            throw ('New untranslated helper prose: '+$row.file+':'+$row.line+': '+$row.literal)
        }
    }
    Remove-Item -LiteralPath (Join-Path $inventoryDirectory 'cable-setup.ps1')
    Remove-Item -LiteralPath (Join-Path $inventoryDirectory 'native-audio-setup.ps1')
    # Parse inert fixture text only: no code in this fixture executes.
    $fixture=@'
throw 'Untranslated error'
Notice "Device $device is unavailable"
return 'Quit ' + $name + ' before continuing.'
[System.Windows.Forms.MessageBox]::Show('Dialog body','Dialog title')
throw (Get-SCSetupText 'Translated error' -Language fr)
# Notice 'Ignored comment'
$name='CABLE'
'@
    # Here-string content is literal PowerShell source, so use plain double quotes.
    $fixture=$fixture.Replace('\"','"')
    [IO.File]::WriteAllText((Join-Path $inventoryDirectory 'fixture.ps1'),$fixture,[Text.UTF8Encoding]::new($false))
    $fixtureAudit=(& $inventoryScript -SourceDirectory $inventoryDirectory) | ConvertFrom-Json
    $expected=@('Untranslated error','Device $device is unavailable','Quit ',' before continuing.','Dialog body','Dialog title')
    if ($fixtureAudit.candidates.Count -ne $expected.Count) { throw 'Helper prose audit fixture count mismatch' }
    foreach ($value in $expected) {
        if (@($fixtureAudit.candidates | Where-Object {$_.literal -ceq $value}).Count -ne 1) { throw ('Helper prose audit missed: '+$value) }
    }
    if ($fixtureAudit.wholeInterfaceCoverageProven) { throw 'Candidate audit incorrectly claims total coverage' }
    Write-Output ('PASS: helper prose regression guard; '+$audit.candidates.Count+' existing candidates remain explicitly unresolved')
} finally { Remove-Item -LiteralPath $inventoryDirectory -Recurse -Force -ErrorAction SilentlyContinue }
