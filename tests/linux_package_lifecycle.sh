#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Run as root only in a disposable container or independent VM.
set -euo pipefail
out=${1:?Output directory required}
mkdir -p "$out"
product=$(sed -n 's/^project(\([^ ]*\) VERSION.*/\1/p' CMakeLists.txt)
if [[ $product == soundcurrent-eq ]]; then desktop=io.github.rhamenator.SoundCurrentEQ.desktop; else desktop=io.github.rhamenator.SoundCurrentStudio.desktop; fi
if command -v apt-get >/dev/null; then
    package=$(find "$PWD/dist" -maxdepth 1 -name '*.deb' | head -1)
    install_package(){ apt-get install --yes "$package"; }
    update_package(){ apt-get install --reinstall --yes "$package"; }
    remove_package(){ apt-get remove --yes "$product"; }
    build_dir=build
else
    package=$(find "$PWD/dist" -name '*.rpm' | head -1)
    install_package(){ dnf install -y "$package"; }
    update_package(){ dnf reinstall -y "$package"; }
    remove_package(){ dnf remove -y "$product"; }
    build_dir='' # RPM %check runs CTest before rpmbuild retires its build tree.
fi
run_ui(){
    if command -v xvfb-run >/dev/null; then xvfb-run -a -s '-screen 0 1280x800x24' "/usr/bin/$product" --ui-self-test
    else QT_QPA_PLATFORM=offscreen "/usr/bin/$product" --ui-self-test; fi
}
[[ -f $package ]]
cp "$package" "$package.sha256" "$out/"
(cd "$(dirname "$package")" && sha256sum --check "$(basename "$package").sha256")
if [[ -n $build_dir ]]; then ctest --test-dir "$build_dir" --output-on-failure --no-tests=error | tee "$out/ctest.log"
else printf 'CTest executed by RPM %%check; see build.log.\n' > "$out/ctest.log"; fi
install_package > "$out/install.log" 2>&1
[[ -x /usr/bin/$product && -f /usr/share/applications/$desktop ]]
grep -q "Exec=$product" "/usr/share/applications/$desktop"
mkdir -p "$HOME/.config/SoundCurrent"
config="$HOME/.config/SoundCurrent/$product.conf"
printf '[General]\nreleaseValidationMarker=keep-this-setting\n' > "$config"
config_hash=$(sha256sum "$config" | cut -d ' ' -f1)
SOUNDCURRENT_UI_SCREENSHOT_DIR="$out/ui" run_ui > "$out/installed-ui.log" 2>&1
update_package > "$out/update.log" 2>&1
[[ $(sha256sum "$config" | cut -d ' ' -f1) == "$config_hash" ]]
remove_package > "$out/uninstall.log" 2>&1
[[ ! -e /usr/bin/$product && ! -e /usr/share/applications/$desktop ]]
[[ $(sha256sum "$config" | cut -d ' ' -f1) == "$config_hash" ]]
install_package > "$out/reinstall.log" 2>&1
run_ui > "$out/reinstalled-ui.log" 2>&1
[[ $(sha256sum "$config" | cut -d ' ' -f1) == "$config_hash" ]]
printf 'PASS: %s install, menu icon, installed GUI controls, in-place reinstall, uninstall cleanup, config preservation and reinstall\n' "$product" | tee "$out/result.txt"
