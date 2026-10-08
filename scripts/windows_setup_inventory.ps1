# SPDX-License-Identifier: GPL-3.0-only
param([Parameter(Mandatory=$true)][string]$SourceDirectory)
$ErrorActionPreference='Stop'
$rows=@()
foreach ($file in Get-ChildItem -LiteralPath $SourceDirectory -Filter '*.ps1') {
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw ('PowerShell parse failed: '+$file.Name) }
    # This is a candidate audit, not a claim that all UI text is extracted.
    $marked=@{}
    foreach ($call in $ast.FindAll({param($node)
        $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -in @('Get-SCSetupText','Format-SCSetupText')
    },$true)) {
        if ($call.CommandElements.Count -gt 1 -and $call.CommandElements[1] -is [Management.Automation.Language.StringConstantExpressionAst]) {
            $marked[$call.CommandElements[1].Extent.StartOffset]=$true
        }
    }
    foreach ($node in $ast.FindAll({param($node)
        $node -is [Management.Automation.Language.StringConstantExpressionAst] -or
        $node -is [Management.Automation.Language.ExpandableStringExpressionAst]
    },$true)) {
        # Raw prose anywhere in the helper, including returned captions and
        # concatenated text, not only direct throw/Notice arguments.
        if ($marked.ContainsKey($node.Extent.StartOffset)) { continue }
        $value=$node.Value
        if ($value -notmatch '[A-Za-z]{2,}\s|\s[A-Za-z]{2,}') { continue }
        $rows += [pscustomobject]@{file=$file.Name;line=$node.Extent.StartLineNumber;kind=$node.GetType().Name;literal=$value}
    }
}
ConvertTo-Json -Depth 8 -InputObject ([pscustomobject]@{
    scope='PowerShell prose candidates; includes identifiers requiring review';
    wholeInterfaceCoverageProven=$false;
    candidates=@($rows | Sort-Object file,line,literal)
})
