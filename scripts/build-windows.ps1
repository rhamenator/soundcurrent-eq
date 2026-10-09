# SPDX-License-Identifier: GPL-3.0-only
param([Parameter(Mandatory=$true)][string]$QtPrefix,
      [ValidateSet('Cable','Native')][string]$AudioRoute = 'Cable',
      [string]$CablePackage,
      [string]$SignedDriverPackage,
      [string]$SignedDriverManager,
      [string]$Nsis = 'C:\Program Files (x86)\NSIS\makensis.exe')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if ($AudioRoute -eq 'Native') {
    $SignedDriverPackage = (Resolve-Path -LiteralPath $SignedDriverPackage).Path
    $SignedDriverManager = (Resolve-Path -LiteralPath $SignedDriverManager).Path
    if ((Get-AuthenticodeSignature -LiteralPath $SignedDriverManager).Status -ne 'Valid') {
        throw 'A signed build of the SoundCurrent driver manager is required for a release installer.'
    }
    & $SignedDriverManager --verify $SignedDriverPackage
    if ($LASTEXITCODE -ne 0) { throw 'The driver package is incomplete or its signature cannot be verified.' }
    } else {
        if (!$CablePackage) { $CablePackage = Join-Path $root '.cache\windows-package\VBCABLE_Driver_Pack45.zip' }
        if (!(Test-Path -LiteralPath $CablePackage)) {
            New-Item -ItemType Directory -Force (Split-Path $CablePackage -Parent) | Out-Null
            & curl.exe --fail --silent --show-error --location --connect-timeout 15 --max-time 90 --output $CablePackage 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip'
            if ($LASTEXITCODE -ne 0) { throw 'Official VB-CABLE package download failed' }
        }
        $CablePackage = (Resolve-Path -LiteralPath $CablePackage).Path
        if ((Get-FileHash -LiteralPath $CablePackage -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb') { throw 'VB-CABLE package checksum mismatch' }
    }
    $version = [regex]::Match((Get-Content CMakeLists.txt -Raw), '(?m)^project\(soundcurrent-eq VERSION ([0-9.]+)').Groups[1].Value
    $stage = Join-Path $root 'build-windows-native\package'
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $major = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion).Split('.')[0]
    if ($major -eq '18') { $generator = 'Visual Studio 18 2026' }
    elseif ($major -eq '17') { $generator = 'Visual Studio 17 2022' }
    else { throw 'Visual Studio 2022 or 2026 C++ tools are required.' }
    & cmake -S . -B build-windows-native -G $generator -A x64 "-DCMAKE_PREFIX_PATH=$QtPrefix"
    if ($LASTEXITCODE -ne 0) { throw 'Windows configure failed' }
    & cmake --build build-windows-native --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Force $stage | Out-Null
    & python scripts/windows_setup_catalogs.py --output "$stage\setup-translations.json"
    if ($LASTEXITCODE -ne 0) { throw 'Audio helper translation export failed' }
    Copy-Item packaging\windows\setup-localization.ps1 $stage
    & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File tests/windows_setup_localization.ps1 -CatalogPath "$stage\setup-translations.json"
    if ($LASTEXITCODE -ne 0) { throw 'Audio helper localization regression failed' }
    Copy-Item build-windows-native\Release\soundcurrent-eq.exe $stage
    Copy-Item build-windows-native\Release\soundcurrent-route-guardian.exe $stage
    # This must be the signed manager built from this release source.
    if ($AudioRoute -eq 'Native') { Copy-Item -LiteralPath $SignedDriverManager -Destination "$stage\soundcurrent-driver-manager.exe" }
    Copy-Item build-windows-native\Release\soundcurrent-cable-setup-guard.exe $stage
    & "$QtPrefix\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --no-compiler-runtime "$stage\soundcurrent-eq.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Qt runtime deployment failed' }
    Copy-Item "$QtPrefix\plugins\platforms\qoffscreen.dll" "$stage\platforms"
    # App-local redistributable DLLs avoid another privileged installer. UCRT is
    # part of supported Windows versions. Refresh these with each app release.
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $redist = Get-ChildItem "$vs\VC\Redist\MSVC" -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } | Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Get-ChildItem (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' } | Select-Object -ExpandProperty FullName -First 1
    if (!$redist) { throw 'Visual Studio redistributable CRT not found' }
    Copy-Item "$redist\*.dll" $stage
    # Run with the same app-local Qt/CRT dependencies deployed for the product.
    # The build directory does not contain Qt runtime DLLs and is not on SDK PATH.
    Copy-Item build-windows-native\Release\soundcurrent-backend-error-text-test.exe $stage
    & "$stage\soundcurrent-backend-error-text-test.exe" --powershell-output-fixture powershell.exe "$root\tests\windows_setup_output_fixture.ps1"
    if ($LASTEXITCODE -ne 0) { throw "Audio helper UTF-8 process output regression failed (exit $LASTEXITCODE)" }
    Remove-Item "$stage\soundcurrent-backend-error-text-test.exe"
    @('[Paths]', 'Prefix=.', 'Plugins=.') | Set-Content -Encoding ascii "$stage\qt.conf"
    New-Item -ItemType Directory -Force "$stage\licenses" | Out-Null
    $sourceArchive = Join-Path $root '.cache\qtbase-everywhere-src-6.12.0.tar.xz'
    New-Item -ItemType Directory -Force (Split-Path $sourceArchive -Parent) | Out-Null
    if (!(Test-Path $sourceArchive)) {
        Write-Output 'Downloading the corresponding Qt source archive'
        $downloaded = $false
        try {
            & curl.exe --fail --silent --show-error --location --connect-timeout 15 --max-time 90 --output $sourceArchive 'https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtbase-everywhere-src-6.12.0.tar.xz'
            $downloaded = $LASTEXITCODE -eq 0
        } catch { $downloaded = $false }
        if (!$downloaded) {
            # This mirror is advertised by Qt's download service. The same
            # pinned checksum applies, regardless of which host supplied it.
            Remove-Item $sourceArchive -ErrorAction SilentlyContinue
            & curl.exe --fail --silent --show-error --location --connect-timeout 15 --max-time 180 --output $sourceArchive 'https://qt.mirror.constant.com/archive/qt/6.12/6.12.0/submodules/qtbase-everywhere-src-6.12.0.tar.xz'
            if ($LASTEXITCODE -ne 0) { throw 'Qt source download failed' }
        }
    }
    if ((Get-FileHash $sourceArchive).Hash.ToLowerInvariant() -ne 'a951bd163c7b80fc6b8c88d7668fb56abf91c152373e13c10666763238131307') { throw 'Qt source checksum mismatch' }
    $sourceDir = Join-Path $root 'build-windows-native\qt-source'
    New-Item -ItemType Directory -Force $sourceDir | Out-Null
    # Only unpack notices here; the complete corresponding source archive is
    # published alongside the installer. Avoid unpacking thousands of unused
    # source files on the Windows runner.
    Write-Output 'Extracting Qt license and attribution notices'
    & 7z x -y "-o$sourceDir" $sourceArchive | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Qt source decompression failed' }
    $sourceTar = Join-Path $sourceDir ([IO.Path]::GetFileNameWithoutExtension($sourceArchive))
    & 7z x -y "-o$sourceDir" $sourceTar '-ir!LICENSE*' '-ir!LICENCE*' '-ir!COPYING*' '-ir!COPYRIGHT*' '-ir!qt_attribution.json' '-ir!*/LICENSES/*' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Qt source extraction failed' }
    $qtSource = Join-Path $sourceDir 'qtbase-everywhere-src-6.12.0'
    if (!(Test-Path "$qtSource\LICENSES")) { throw 'Qt license notices missing' }
    Get-ChildItem $qtSource -Recurse -File | Where-Object { $_.Name -match '^(LICENSE|LICENCE|COPYING|COPYRIGHT)' -or $_.Name -eq 'qt_attribution.json' -or $_.FullName.Contains('\LICENSES\') } | ForEach-Object {
        $relative = $_.FullName.Substring($qtSource.Length + 1)
        $dest = Join-Path "$stage\licenses\Qt" $relative
        New-Item -ItemType Directory -Force (Split-Path $dest -Parent) | Out-Null
        Copy-Item $_.FullName $dest
    }
    Copy-Item THIRD-PARTY-NOTICES.md "$stage\licenses"
    # Tests use the same private DLLs and Qt plugins shipped to users.
    Copy-Item build-windows-native\Release\soundcurrent-dsp-test.exe $stage
    Write-Output 'Checking shared DSP and Qt controls'
    $dsp = Start-Process "$stage\soundcurrent-dsp-test.exe" -PassThru -NoNewWindow
    $null = $dsp.Handle
    if (!$dsp.WaitForExit(60000)) { Stop-Process -Id $dsp.Id -Force; throw 'DSP test timed out' }
    $dsp.Refresh()
    if ($null -eq $dsp.ExitCode -or $dsp.ExitCode -ne 0) { throw "DSP test failed: $($dsp.ExitCode)" }
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_FORCE_STDERR_LOGGING = '1' # Capture Qt test failures from GUI-linked processes.
    foreach ($testName in @('soundcurrent-equipment-test', 'soundcurrent-processing-guard-test', 'soundcurrent-enhancement-test', 'soundcurrent-update-test','soundcurrent-spin-test','soundcurrent-localization-test','soundcurrent-startup-test')) {
        # Text shaping needs system fonts. Qt's Windows offscreen plugin
        # searches an unbundled lib/fonts directory instead of native fonts.
        $env:QT_QPA_PLATFORM = if ($testName -eq 'soundcurrent-localization-test') { 'windows' } else { 'offscreen' }
        Copy-Item "build-windows-native\Release\$testName.exe" $stage
        $testArgs = @()
        if ($testName -eq 'soundcurrent-equipment-test') { $testArgs = @('--ui-self-test') }
        $testLog = Join-Path $stage "$testName.stderr.log"
        if ($testArgs.Count) { $test = Start-Process "$stage\$testName.exe" -ArgumentList $testArgs -PassThru -NoNewWindow -RedirectStandardError $testLog }
        else { $test = Start-Process "$stage\$testName.exe" -PassThru -NoNewWindow -RedirectStandardError $testLog }
        $null = $test.Handle
        if (!$test.WaitForExit(90000)) { Stop-Process -Id $test.Id -Force; throw "$testName timed out" }
        $test.Refresh()
        if ($null -eq $test.ExitCode -or $test.ExitCode -ne 0) {
            Get-Content $testLog -ErrorAction SilentlyContinue | Write-Output
            throw "$testName failed: $($test.ExitCode)"
        }
        Remove-Item $testLog
        Remove-Item "$stage\$testName.exe"
    }
    $env:QT_QPA_PLATFORM = 'offscreen'
    $uiLog = Join-Path $root 'build-windows-native\ui-self-test.log'
    $ui = Start-Process "$stage\soundcurrent-eq.exe" -ArgumentList '--ui-self-test' -PassThru -RedirectStandardError $uiLog
    $null = $ui.Handle
    if (!$ui.WaitForExit(90000)) {
        Stop-Process -Id $ui.Id -Force
        Get-Content $uiLog -ErrorAction SilentlyContinue
        throw 'Shared UI test timed out'
    }
    $ui.Refresh()
    Get-Content $uiLog -ErrorAction SilentlyContinue
    foreach ($locale in @('de','fr','es','it','pt-PT','pt-BR','nl','pl','cs','sk','uk','ru','el','tr','sv','da','nb','fi','ro','hu','nn','ar','he','fa','zh-Hans','zh-Hant','ja','ko','hi','id','vi','th','sw','qps-ploc','qps-rtl')) {
        $localizedLog = Join-Path $root "build-windows-native\localized-$locale.log"
        $localized = Start-Process "$stage\soundcurrent-eq.exe" -ArgumentList @('--localization-ui-test','--language',$locale) -PassThru -RedirectStandardError $localizedLog
        $null = $localized.Handle
        if (!$localized.WaitForExit(60000)) { Stop-Process -Id $localized.Id -Force; throw "Localized UI timed out: $locale" }
        $localized.Refresh()
        if ($localized.ExitCode -ne 0) { Get-Content $localizedLog; throw "Localized UI failed: $locale" }
    }
    Copy-Item 'build-windows-native\Release\soundcurrent-equipment-ui-test.exe' $stage
    foreach ($locale in @('en','fr','de','es','it','pt-PT','pt-BR','nl','pl')) {
        $equipmentLog = Join-Path $root "build-windows-native\equipment-ui-$locale.log"
        $equipment = Start-Process "$stage\soundcurrent-equipment-ui-test.exe" -ArgumentList @('--language',$locale) -PassThru -RedirectStandardError $equipmentLog
        $null = $equipment.Handle
        if (!$equipment.WaitForExit(60000)) { Stop-Process -Id $equipment.Id -Force; throw "Equipment UI timed out: $locale" }
        $equipment.Refresh()
        if ($equipment.ExitCode -ne 0) { Get-Content $equipmentLog; throw "Equipment UI failed: $locale" }
    }
    Remove-Item "$stage\soundcurrent-equipment-ui-test.exe"
    # Exercise only inert setup fixtures: no driver installation or endpoint changes.
    $setupScript = Join-Path $stage 'audio-setup.ps1'
    $setupBackup = Join-Path $stage 'audio-setup.saved.ps1'
    $hadSetupScript = Test-Path $setupScript
    if ($hadSetupScript) { Move-Item $setupScript $setupBackup }
    try {
        foreach ($locale in @('fr','de','es')) {
            $expected = if ($locale -eq 'fr') { 'introuvable' } elseif ($locale -eq 'de') { 'fehlt' } else { 'encuentra' }
            $setup = Start-Process "$stage\soundcurrent-eq.exe" -ArgumentList @('--windows-audio-setup-test','install',$expected,'--language',$locale) -PassThru -NoNewWindow
            $null = $setup.Handle
            if (!$setup.WaitForExit(45000)) { Stop-Process -Id $setup.Id -Force; throw "Missing setup test timed out: $locale" }
            $setup.Refresh()
            if ($setup.ExitCode -ne 0) { throw "Missing setup translation failed: $locale" }
        }
        'Write-Output "Fixture technical diagnostic"; exit 3010' | Set-Content -Encoding ascii $setupScript
        foreach ($locale in @('fr','de','es')) {
            $expected = if ($locale -eq 'fr') { 'Redémarrez' } elseif ($locale -eq 'de') { 'Starten' } else { 'Reinicie' }
            $setup = Start-Process "$stage\soundcurrent-eq.exe" -ArgumentList @('--windows-audio-setup-test','install',$expected,'--language',$locale) -PassThru -NoNewWindow
            $null = $setup.Handle
            if (!$setup.WaitForExit(45000)) { Stop-Process -Id $setup.Id -Force; throw "Restart setup test timed out: $locale" }
            $setup.Refresh()
            if ($setup.ExitCode -ne 0) { throw "Restart instruction translation failed: $locale" }
        }
    } finally {
        Remove-Item $setupScript -ErrorAction SilentlyContinue
        if ($hadSetupScript) { Move-Item $setupBackup $setupScript }
    }
    & python tests/translation_catalogs.py
    if ($LASTEXITCODE -ne 0) { throw 'Translation-maintenance regression tests failed' }
    & python scripts/localization.py --check
    if ($LASTEXITCODE -ne 0) { throw 'Translation catalog audit failed' }
    Remove-Item Env:\QT_QPA_PLATFORM
    if ($null -eq $ui.ExitCode -or $ui.ExitCode -ne 0) { throw "Shared UI test failed: $($ui.ExitCode)" }
    Remove-Item "$stage\soundcurrent-dsp-test.exe"
    # Generate an exact payload deletion manifest, retaining unknown user files.
    $delete = @()
    Get-ChildItem $stage -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($stage.Length + 1)
        $delete += 'Delete "$INSTDIR\' + $relative + '"'
    }
    Get-ChildItem $stage -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
        $delete += 'RMDir "$INSTDIR\' + $_.FullName.Substring($stage.Length + 1) + '"'
    }
    $delete | Set-Content -Encoding utf8 build-windows-native\uninstall-payload.nsh
    New-Item -ItemType Directory -Force dist | Out-Null
    $installer = Join-Path $root "dist\SoundCurrent-EQ-$version-windows-x64-setup.exe"
    [string[]]$audioOptions = if ($AudioRoute -eq 'Native') { @("/DDRIVER_DIR=$SignedDriverPackage") } else { @("/DCABLE_ZIP=$CablePackage") }
    $installerScript = if ($AudioRoute -eq 'Native') { 'packaging\windows\soundcurrent-eq-native.nsi' } else { 'packaging\windows\soundcurrent-eq.nsi' }
    & python tests/installer_language_activation.py
    if ($LASTEXITCODE -ne 0) { throw 'Installer language activation regression failed' }
    $generatedInstallerScript = Join-Path $root 'build-windows-native\localized-installer.nsi'
    & python scripts/windows_installer_catalogs.py --product "SoundCurrent EQ" --output "$root\build-windows-native\installer-translations.json" --installer-source $installerScript --installer-output $generatedInstallerScript --activate-languages
    if ($LASTEXITCODE -ne 0) { throw 'Installer caption generation failed' }
    & $Nsis /INPUTCHARSET UTF8 "/DAPP_EXE=$stage\soundcurrent-eq.exe" "/DDLL_DIR=$stage" "/DAPP_VERSION=$version" "/DOUTPUT=$installer" @audioOptions "/DSOURCE_ROOT=$root" "/DUNINSTALL_PAYLOAD=$root\build-windows-native\uninstall-payload.nsh" $generatedInstallerScript
    if ($LASTEXITCODE -ne 0) { throw 'Windows installer build failed' }
    Copy-Item $sourceArchive dist
    $sourceHash = (Get-FileHash $sourceArchive).Hash.ToLowerInvariant()
    "$sourceHash  $(Split-Path $sourceArchive -Leaf)" | Set-Content -Encoding ascii "dist\$(Split-Path $sourceArchive -Leaf).sha256"
    $hash = (Get-FileHash $installer).Hash.ToLowerInvariant()
    "$hash  $(Split-Path $installer -Leaf)" | Set-Content -Encoding ascii "$installer.sha256"
    Write-Output $installer
} finally { Pop-Location }
