#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# System-changing: use only on an independent Ubuntu/Debian test VM.
set -euo pipefail
[[ ${1:-} == --run && -f ${2:-} ]] || { echo 'Usage: test --run PACKAGE.deb RESULT_DIRECTORY'; exit 2; }
package=$(realpath "$2")
app=$(dpkg-deb -f "$package" Package)
case "$app" in soundcurrent-eq|soundcurrent-studio) ;; *) echo 'Unexpected package'; exit 2;; esac
[[ -n ${3:-} ]] || exit 2
result=$(realpath -m "$3");mkdir -p "$result"
if pgrep -f "(^|/)$app( |$)" >/dev/null; then echo 'Quit the tested app first';exit 1;fi
nonce=$(cat /proc/sys/kernel/random/uuid)
config="$HOME/.config/SoundCurrent/localization-qa-$nonce.conf"
profile="$HOME/.local/share/SoundCurrent/$app/localization-qa-$nonce.json"
unowned="/usr/share/doc/$app/localization-qa-$nonce.txt"
mkdir -p "$(dirname "$config")" "$(dirname "$profile")"
printf 'preserve-settings\n' > "$config"
printf '{"id":"localization-qa","gain":-12.5}\n' > "$profile"
printf 'preserve-user-file\n' | sudo -n tee "$unowned" >/dev/null
cleanup(){ rm -f -- "$config" "$profile";sudo -n rm -f -- "$unowned"; }
trap cleanup EXIT
originalConfig=$(sha256sum "$config");originalProfile=$(sha256sum "$profile")
sudo -n env DEBIAN_FRONTEND=noninteractive apt-get install --reinstall -y "$package" > "$result/update.log" 2>&1
[[ $(sha256sum "$config") == "$originalConfig" && $(sha256sum "$profile") == "$originalProfile" ]]
[[ $(cat "$unowned") == preserve-user-file ]]
sudo -n env DEBIAN_FRONTEND=noninteractive apt-get remove -y "$app" > "$result/remove.log" 2>&1
[[ ! -e "/usr/bin/$app" ]]
[[ $(sha256sum "$config") == "$originalConfig" && $(sha256sum "$profile") == "$originalProfile" ]]
[[ $(cat "$unowned") == preserve-user-file ]]
sudo -n env DEBIAN_FRONTEND=noninteractive apt-get install -y "$package" > "$result/reinstall.log" 2>&1
[[ -x "/usr/bin/$app" ]]
[[ $(sha256sum "$config") == "$originalConfig" && $(sha256sum "$profile") == "$originalProfile" ]]
QT_QPA_PLATFORM=offscreen SOUNDCURRENT_TEST_FORMAT_LOCALE=de-DE "/usr/bin/$app" --localization-ui-test --language ar > "$result/reinstalled-ui.log" 2>&1
grep -F 'Localization UI: ar -> ar' "$result/reinstalled-ui.log" >/dev/null
sha256sum "$package" > "$result/package.sha256"
printf 'PASS: %s update/remove/reinstall, settings/profile fixtures and unknown user file preserved\n' "$app" | tee "$result/result.txt"
