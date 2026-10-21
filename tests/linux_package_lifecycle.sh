#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Run as root only in a disposable container or independent VM.
set -euo pipefail
out=${1:?Output directory required}
mkdir -p "$out"
product=$(sed -n 's/^project(\([^ ]*\) VERSION.*/\1/p' CMakeLists.txt)
if [[ $product == soundcurrent-eq ]]; then desktop=io.github.rhamenator.SoundCurrentEQ.desktop; else desktop=io.github.rhamenator.SoundCurrentStudio.desktop; fi
version=$(sed -n 's/^project([^ ]* VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[[ -n $version ]]
if command -v apt-get >/dev/null; then
    package="$PWD/dist/${product}_${version}_$(dpkg --print-architecture).deb"
    [[ $(dpkg-deb -f "$package" Package) == "$product" && $(dpkg-deb -f "$package" Version) == "$version" ]]
    install_package(){ apt-get install --yes "$package"; }
    update_package(){ apt-get install --reinstall --yes "$package"; }
    remove_package(){ apt-get remove --yes "$product"; }
    build_dir=build
else
    mapfile -t candidates < <(find "$PWD/dist" -type f -name "${product}-${version}-*.rpm")
    [[ ${#candidates[@]} == 1 ]] || { echo 'Expected exactly one current-version RPM'; exit 1; }
    package=${candidates[0]}
    [[ $(rpm -qp --queryformat '%{NAME}' "$package") == "$product" && $(rpm -qp --queryformat '%{VERSION}' "$package") == "$version" ]]
    install_package(){ dnf install -y "$package"; }
    update_package(){ dnf reinstall -y "$package"; }
    remove_package(){ dnf remove -y "$product"; }
    build_dir='' # RPM %check runs CTest before rpmbuild retires its build tree.
fi
run_ui(){
    if command -v xvfb-run >/dev/null; then xvfb-run -a -s '-screen 0 1280x800x24' "/usr/bin/$product" --ui-self-test
    else QT_QPA_PLATFORM=offscreen "/usr/bin/$product" --ui-self-test; fi
}
check_launcher(){
    local phase=$1
    # Exact installed metadata includes all localized descriptions and preserves
    # product identity, Exec arguments, icon, categories and locale aliases.
    cmp "data/$desktop" "/usr/share/applications/$desktop"
    cp "/usr/share/applications/$desktop" "$out/launcher-$phase.desktop"
    sha256sum "$out/launcher-$phase.desktop" > "$out/launcher-$phase.sha256"
}
[[ -f $package ]]
cp "$package" "$package.sha256" "$out/"
(cd "$(dirname "$package")" && sha256sum --check "$(basename "$package").sha256")
if [[ -n $build_dir ]]; then ctest --test-dir "$build_dir" --output-on-failure --no-tests=error | tee "$out/ctest.log"
else printf 'CTest executed by RPM %%check; see build.log.\n' > "$out/ctest.log"; fi
install_package > "$out/install.log" 2>&1
[[ -x /usr/bin/$product && -f /usr/share/applications/$desktop ]]
grep -q "Exec=$product" "/usr/share/applications/$desktop"
check_launcher installed
mkdir -p "$HOME/.config/SoundCurrent"
config="$HOME/.config/SoundCurrent/$product.conf"
printf '[General]\nreleaseValidationMarker=keep-this-setting\n' > "$config"
config_hash=$(sha256sum "$config" | cut -d ' ' -f1)
SOUNDCURRENT_UI_SCREENSHOT_DIR="$out/ui" run_ui > "$out/installed-ui.log" 2>&1
for locale in de fr es it pt-PT pt-BR nl pl cs sk uk ru el tr sv da nb fi ro hu nn ar he fa zh-Hans zh-Hant ja ko hi id vi th sw; do
    timeout 60s env QT_QPA_PLATFORM=offscreen "/usr/bin/$product" --localization-ui-test --language "$locale" > "$out/locale-$locale.log" 2>&1
    grep -F "Localization UI: $locale -> $locale" "$out/locale-$locale.log" >/dev/null
done
update_package > "$out/update.log" 2>&1
python3 tests/linux_easy_installer_lifecycle.py "$package" "$out"
check_launcher updated
[[ $(sha256sum "$config" | cut -d ' ' -f1) == "$config_hash" ]]
remove_package > "$out/uninstall.log" 2>&1
[[ ! -e /usr/bin/$product && ! -e /usr/share/applications/$desktop ]]
[[ $(sha256sum "$config" | cut -d ' ' -f1) == "$config_hash" ]]
install_package > "$out/reinstall.log" 2>&1
check_launcher reinstalled
run_ui > "$out/reinstalled-ui.log" 2>&1
[[ $(sha256sum "$config" | cut -d ' ' -f1) == "$config_hash" ]]
printf 'PASS: %s install, menu icon, installed GUI controls, in-place reinstall, uninstall cleanup, config preservation and reinstall\n' "$product" | tee "$out/result.txt"
