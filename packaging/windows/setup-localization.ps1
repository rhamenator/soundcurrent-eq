# SPDX-License-Identifier: GPL-3.0-only
# Data-only lookup; no driver, endpoint, registry-write or process actions.
$script:SCSetupCatalogPath = Join-Path $PSScriptRoot 'setup-translations.json'
$script:SCSetupLanguages = $null
function Resolve-SCSetupLanguage([string]$Requested, $Available) {
    $tag = $Requested.Trim().Replace('_','-')
    foreach ($key in $Available) { if ($key -ieq $tag) { return $key } }
    if ($tag -notmatch '^[a-zA-Z]{2,3}(?:-[a-zA-Z]{2}|-[0-9]{3})?$') { return 'en' }
    $parts = $tag.Split('-')
    if ($parts[0] -ieq 'zh') {
        if ($parts.Count -eq 1 -or $parts[1] -in @('CN','SG')) { return 'zh-Hans' }
        if ($parts[1] -in @('TW','HK','MO')) { return 'zh-Hant' }
        return 'en'
    }
    # Region-specific Portuguese packs require a matching region.
    if ($parts[0] -ieq 'pt') { return 'en' }
    foreach ($key in $Available) { if ($key -ieq $parts[0]) { return $key } }
    return 'en'
}
function Get-SCSetupText([string]$Source, [string]$Language = '', [string]$Application = '') {
    try {
        if ($null -eq $script:SCSetupLanguages) {
            $file = Get-Item -LiteralPath $script:SCSetupCatalogPath -ErrorAction Stop
            if ($file.Length -gt 8388608) { return $Source }
            $data = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($data.schema -ne 1) { return $Source }
            $script:SCSetupLanguages = $data.languages
        }
        if (!$Language -and $Application -and $IsWindows -ne $false) {
            # Windows PowerShell 5.1 has no IsWindows variable.
            $setting = Get-ItemProperty -LiteralPath "HKCU:\Software\SoundCurrent\$Application\i18n" -Name language -ErrorAction SilentlyContinue
            if ($setting.language -and $setting.language -ne 'system') { $Language = $setting.language }
        }
        if (!$Language) { $Language = (Get-UICulture).Name }
        $available = @($script:SCSetupLanguages.PSObject.Properties.Name)
        $tag = Resolve-SCSetupLanguage $Language $available
        $pack = $script:SCSetupLanguages.PSObject.Properties[$tag].Value
        $entry = @($pack.PSObject.Properties | Where-Object {
            [string]::Equals($_.Name,$Source,[StringComparison]::Ordinal)
        }) | Select-Object -First 1
        if ($null -eq $entry -or $entry.Value -isnot [string] -or !$entry.Value.Trim()) { return $Source }
        return $entry.Value
    } catch { return $Source } # Missing/corrupt/older package data keeps readable English.
}

function Format-SCSetupText([string]$Source, [string[]]$Values,
                            [string]$Language = '', [string]$Application = '') {
    # This API is for owned templates, never arbitrary caught external errors.
    # Only Qt's numbered %1..%99 form is supported. Numbers/codes are supplied
    # as invariant data; localized %L or numerus semantics require a separate API.
    $unsupported = '%L[0-9]+|%Ln|%n'
    if ($Source -match $unsupported) { throw 'Unsupported setup template placeholder.' }
    $pattern = '%[1-9][0-9]*'
    $sourceTokens = @([regex]::Matches($Source,$pattern) | ForEach-Object {$_.Value})
    foreach ($token in $sourceTokens) {
        $number = [int]::Parse($token.Substring(1),[Globalization.CultureInfo]::InvariantCulture)
        if ($number -gt 99 -or $number -gt $Values.Count) { throw 'Missing setup template value.' }
    }
    $template = Get-SCSetupText $Source -Language $Language -Application $Application
    $targetTokens = @([regex]::Matches($template,$pattern) | ForEach-Object {$_.Value})
    if ($template -match $unsupported -or
        [string]::Join('|',@($sourceTokens | Sort-Object)) -cne [string]::Join('|',@($targetTokens | Sort-Object))) {
        $template = $Source # Corrupt/stale translated placeholders must not lose data.
    }
    $replacement = {param($match)
        $index = [int]::Parse($match.Value.Substring(1),[Globalization.CultureInfo]::InvariantCulture) - 1
        return [string]$Values[$index]
    }.GetNewClosure()
    return [regex]::Replace($template,$pattern,[Text.RegularExpressions.MatchEvaluator]$replacement)
}
