# SPDX-License-Identifier: GPL-3.0-only
# Inert test of the actual setup function. No registry, device, UAC or Forms use.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'packaging/windows/cable-setup.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Setup script does not parse'}
$definition=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Configure-CableVolume'},$true))
if($definition.Count -ne 1){throw 'Configuration entry point missing'}
Invoke-Expression $definition[0].Extent.Text
function Join-Path { return 'InertNativeGuard' }
function Test-Path { return $true }
function InertNativeGuard { $script:action=$args[0]; $global:LASTEXITCODE=$script:fixtureCode }
function Get-SCSetupText {param($Source,$Language,$Application) return $Source}
function Format-SCSetupText {param($Source,$Values,$Language,$Application) return $Source.Replace('%1',$Values[0])}
function Notice {param($Text) $script:notice=$Text}
$App='studio';$Language='en';$PSScriptRoot='inert'
foreach($silentMode in @($true,$false)){
 $Silent=$silentMode
 foreach($result in @(0,3010,740,1223,31)){
  $script:fixtureCode=$result;$script:notice='';$script:action=''
  $actual=Configure-CableVolume
  if($actual -is [array] -or $actual -ne $result){throw "Status polluted by notice output: $actual"}
  $expected=if($silentMode){'--configure-volume-unattended'}else{'--configure-volume'}
  if($script:action -ne $expected){throw 'Wrong elevation policy'}
  if($result -eq 0 -and $script:notice){throw 'Idempotent success showed notice'}
  if($result -ne 0 -and !$script:notice){throw 'Missing repair/restart message'}
 }
}
Write-Output 'PASS: actual configuration function preserves status, distinguishes restart/failure, and never requests interactive elevation in silent mode'
