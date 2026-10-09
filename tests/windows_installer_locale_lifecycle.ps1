# SPDX-License-Identifier: GPL-3.0-only
# System-changing: use only on a full independent Windows clone, never an original VM.
param([switch]$Run,[Parameter(Mandatory=$true)][ValidateSet('EQ','Studio')][string]$Product,
 [Parameter(Mandatory=$true)][string]$InstallerPath,[string]$InitialInstallerPath,
 [Parameter(Mandatory=$true)][string]$CatalogPath,
 [Parameter(Mandatory=$true)][string]$LanguageMapPath,
 [Parameter(Mandatory=$true)][string]$ResultDirectory,
 [string[]]$LanguageSequence=@('fr','nn','ar'),
 [switch]$QualifyInstalledUi,[string]$ExpectedExecutablePath)
$ErrorActionPreference='Stop'
function Get-LocaleExpectation($Catalog,$Map,[string]$Tag) {
 $row=@($Map.languages | Where-Object {$_.tag -ceq $Tag})
 $entry=$Catalog.languages.PSObject.Properties[$Tag]
 if($row.Count -ne 1 -or !$entry){throw 'Unknown installer locale identity'}
 $pack=$entry.Value.cable
 if(!$pack.shortcutCaptions){throw 'Missing localized shortcut inventory'}
 [pscustomobject]@{tag=$Tag;id=$row[0].windowsLanguageId;captions=$pack.shortcutCaptions}
}
function Read-ShortcutData([string]$Path) {
 # WScript's reader returned empty fields for Arabic filenames in the test guest.
 # Inspect identical bytes under an ASCII name; original presence is checked above.
 $temporary=Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString('N')+'.lnk')
 if($temporary -match '[^\x00-\x7F]'){throw 'Shortcut fixture requires an ASCII temporary path'}
 $shell=$null;$shortcut=$null
 try {
  Copy-Item -LiteralPath $Path -Destination $temporary
  if((Get-FileHash -LiteralPath $Path).Hash -ne (Get-FileHash -LiteralPath $temporary).Hash){throw 'Shortcut copy checksum mismatch'}
  $shell=New-Object -ComObject WScript.Shell
  $shortcut=$shell.CreateShortcut($temporary)
  [pscustomobject]@{TargetPath=$shortcut.TargetPath;Arguments=$shortcut.Arguments}
 }finally {
  if($shortcut){[void][Runtime.InteropServices.Marshal]::ReleaseComObject($shortcut)}
  if($shell){[void][Runtime.InteropServices.Marshal]::ReleaseComObject($shell)}
  Remove-Item -LiteralPath $temporary -ErrorAction SilentlyContinue
 }
}
function Assert($value,$message){if(!$value){throw $message}}
function Execute([string]$file,[string]$arguments){
 $p=Start-Process -FilePath $file -ArgumentList $arguments -PassThru
 try {if(!$p.WaitForExit(90000)){Stop-Process -Id $p.Id -Force;throw 'Installer timed out'}
 $p.Refresh();Assert ($p.ExitCode -eq 0) "Installer failed: $($p.ExitCode)"
 }finally{$p.Dispose()}
}
function AudioState {
 @(Get-CimInstance Win32_PnPEntity -Filter "PNPClass='MEDIA'" | Sort-Object DeviceID |
 Select-Object DeviceID,Service,Status,ConfigManagerErrorCode) | ConvertTo-Json -Compress
}
if(!$Run){Write-Output 'Use -Run only with the cable installer on an independent Windows clone.';exit 0}
Assert ($env:OS -eq 'Windows_NT') 'Windows is required'
Assert ($LanguageSequence.Count -ge 2) 'Provide at least two locales for language-changing update'
$catalog=Get-Content -LiteralPath $CatalogPath -Raw -Encoding UTF8 | ConvertFrom-Json
$map=Get-Content -LiteralPath $LanguageMapPath -Raw -Encoding UTF8 | ConvertFrom-Json
$expectations=@($LanguageSequence | ForEach-Object {Get-LocaleExpectation $catalog $map $_})
$display='SoundCurrent '+$Product
Assert ($catalog.product -ceq $display) 'Caption inventory product mismatch'
$name='soundcurrent-'+$Product.ToLower()
$uninstallKey="HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrent$Product"
$preferenceKey="HKCU:\Software\SoundCurrent\$display"
$settings="HKCU:\Software\SoundCurrent\$name"
Assert (!(Test-Path $uninstallKey)) 'Requires a clean clone with no installed tested product'
# QSettings may create an empty native key while reading absent preferences.
if(Test-Path $preferenceKey){
 $existingPreference=Get-Item -LiteralPath $preferenceKey
 Assert (@($existingPreference.Property).Count -eq 0 -and @($existingPreference.GetSubKeyNames()).Count -eq 0) 'Requires a clean clone without installer preference values or child keys'
}
Assert (!(Test-Path $settings)) 'Requires a clean clone without app settings'
Assert (-not (Get-Process $name -ErrorAction SilentlyContinue)) 'Tested app is running'
$shortcutFolder=Join-Path ([Environment]::GetFolderPath('Programs')) $display
Assert (!(Test-Path $shortcutFolder)) 'Requires a clean clone without product shortcuts'
New-Item -ItemType Directory -Force $ResultDirectory | Out-Null
$before=AudioState
New-Item $preferenceKey -Force | Out-Null
$sentinel='localizationQa_'+[guid]::NewGuid().ToString('N')
$owned=@('Uninstall','Audio driver setup','Install VB-CABLE','VB-CABLE settings')
foreach($pack in $catalog.languages.PSObject.Properties){
 $owned+=@($pack.Value.cable.shortcutCaptions.PSObject.Properties | ForEach-Object {$_.Value})
}
$owned=@($owned | Sort-Object -Unique)
$marker=$null;$shortcutMarker=$null;$steps=@()
try {
 foreach($expected in $expectations){
  New-ItemProperty $preferenceKey -Name InstallerLanguage -Value ([string]$expected.id) -PropertyType String -Force | Out-Null
  $candidate=if($steps.Count -eq 0 -and $InitialInstallerPath){$InitialInstallerPath}else{$InstallerPath}
  Execute $candidate '/S'
  Assert (Test-Path $uninstallKey) 'Product registration missing after install/update'
  $directory=Split-Path (Get-ItemProperty $uninstallKey).DisplayIcon -Parent
  $exe=Join-Path $directory "$name.exe"
  Assert (Test-Path $exe) 'Installed executable missing'
  # Verify cable route before any uninstaller can run. Native ownership release is excluded.
  $helper=Get-Content -LiteralPath (Join-Path $directory 'audio-setup.ps1') -Raw -Encoding UTF8
  Assert ($helper.Contains('soundcurrent-cable-setup-guard') -and !$helper.Contains('soundcurrent-driver-manager')) 'This lifecycle test requires the cable variant'
  Assert ((Get-ItemPropertyValue $preferenceKey InstallerLocale) -ceq $expected.tag) 'Installer catalog tag was not persisted'
  Assert ((Get-ItemPropertyValue $preferenceKey InstallerLanguage) -eq [string]$expected.id) 'Numeric installer language was not persisted'
  $current=@($expected.captions.PSObject.Properties | ForEach-Object {$_.Value})
  foreach($caption in $owned){
   $link=Join-Path $shortcutFolder ($caption+'.lnk')
   Assert ((Test-Path -LiteralPath $link) -eq ($current -contains $caption)) 'Missing current or stale localized shortcut'
  }
  foreach($action in @('SCShortcutSetup','SCShortcutCableSettings')){
   $link=Read-ShortcutData (Join-Path $shortcutFolder ($expected.captions.$action+'.lnk'))
   Assert ($link.TargetPath.EndsWith('\WindowsPowerShell\v1.0\powershell.exe',[StringComparison]::OrdinalIgnoreCase)) 'Helper shortcut target mismatch'
   Assert ($link.Arguments.Contains('-Language "'+$expected.tag+'"')) 'Helper shortcut locale argument mismatch'
  }
  if(!$marker){
   New-Item $settings -Force | Out-Null
   New-ItemProperty $settings -Name $sentinel -Value 'preserve-settings' -PropertyType String | Out-Null
   $marker=Join-Path $directory ($sentinel+'.txt');'preserve-file' | Set-Content -Encoding UTF8 $marker
   $shortcutMarker=Join-Path $shortcutFolder ($sentinel+'.lnk');'preserve-shortcut-folder-file' | Set-Content -Encoding UTF8 $shortcutMarker
  }
  Assert ((Get-ItemPropertyValue $settings $sentinel) -eq 'preserve-settings') 'Update lost settings marker'
  Assert ((Get-Content -LiteralPath $marker) -eq 'preserve-file') 'Update lost unknown user file'
  Assert ((Get-Content -LiteralPath $shortcutMarker) -eq 'preserve-shortcut-folder-file') 'Update lost or changed unrelated shortcut-folder file'
  Assert ((AudioState) -eq $before) 'Install/update changed audio device identities/status'
  $steps+=@{locale=$expected.tag;languageId=$expected.id;installerSha256=(Get-FileHash $candidate).Hash;shortcuts='passed';preservation='passed'}
 }
 $installedUi=@()
 if($QualifyInstalledUi){
  Assert ($ExpectedExecutablePath -and (Test-Path -LiteralPath $ExpectedExecutablePath)) 'Installed UI qualification requires staged executable identity'
  Assert ((Get-FileHash -LiteralPath $exe).Hash -eq (Get-FileHash -LiteralPath $ExpectedExecutablePath).Hash) 'Installed executable differs from staged payload'
  foreach($locale in $catalog.languages.PSObject.Properties.Name){
   $stdout=Join-Path $ResultDirectory ('installed-ui-'+$locale+'.stdout.log')
   $stderr=Join-Path $ResultDirectory ('installed-ui-'+$locale+'.stderr.log')
   $process=Start-Process -FilePath $exe -ArgumentList @('--localization-ui-test','--language',$locale) -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
   try {
    $null=$process.Handle
    if(!$process.WaitForExit(60000)){Stop-Process -Id $process.Id -Force;throw ('Installed locale timed out: '+$locale)}
    $process.Refresh()
    Assert ($process.ExitCode -eq 0) ('Installed locale failed: '+$locale)
    Assert ((Get-Content -LiteralPath $stdout -Raw -Encoding UTF8).Contains('Localization UI: '+$locale+' -> '+$locale)) ('Installed locale identity not reported: '+$locale)
    $installedUi+=$locale
   }finally{$process.Dispose()}
  }
  Assert ($installedUi.Count -eq 34) 'Installed UI qualification requires all catalog languages'
 }
 Execute (Join-Path $directory 'uninstall.exe') '/S'
 $deadline=[DateTime]::UtcNow.AddSeconds(30)
 while((Test-Path $uninstallKey) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100}
 Assert (!(Test-Path $uninstallKey)) 'Uninstaller registration remains'
 Assert (!(Test-Path $exe)) 'Application executable remains'
 foreach($caption in $owned){Assert (!(Test-Path -LiteralPath (Join-Path $shortcutFolder ($caption+'.lnk')))) 'Uninstaller left localized action shortcut'}
 Assert ((Get-ItemPropertyValue $settings $sentinel) -eq 'preserve-settings') 'Uninstall lost settings marker'
 Assert (Test-Path -LiteralPath $marker) 'Uninstall lost unknown user file'
 Assert ((Get-Content -LiteralPath $shortcutMarker) -eq 'preserve-shortcut-folder-file') 'Uninstall lost or changed unrelated shortcut-folder file'
 Assert ((AudioState) -eq $before) 'Silent cable uninstall changed audio device identities/status'
 @{product=$Product;installerSha256=(Get-FileHash $InstallerPath).Hash;steps=$steps;uninstall='passed';installedUiLocales=$installedUi;installedExecutableSha256=$(if($QualifyInstalledUi){(Get-FileHash -LiteralPath $ExpectedExecutablePath).Hash}else{$null});scope='Silent cable app lifecycle on a disposable Windows VM; installed UI catalog fixtures optional, chooser rendering and native-driver lifecycle excluded';nativeSpeakerVerified=$false} | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 (Join-Path $ResultDirectory 'installer-locale-lifecycle.json')
 Write-Output "PASS: $Product locale install/update/uninstall and preservation checks"
}finally {
 if(Test-Path $settings){Remove-ItemProperty $settings -Name $sentinel -ErrorAction SilentlyContinue}
 if($marker){Remove-Item -LiteralPath $marker -ErrorAction SilentlyContinue}
 if($shortcutMarker){Remove-Item -LiteralPath $shortcutMarker -ErrorAction SilentlyContinue}
 # Keep installation and preference evidence after a failed run; do not auto-uninstall a wrong route.
}
