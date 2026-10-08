#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Run only on an independent Ubuntu test VM. Does not start live audio.
set -euo pipefail
[[ ${1:-} == --run ]] || { echo 'Use --run on an independent test VM'; exit 2; }
qa=$(mktemp -d /tmp/soundcurrent-localization-qa.XXXXXX)
printf '%s\n' "$qa" > /tmp/soundcurrent-localization-qa-current
for app in soundcurrent-eq soundcurrent-studio; do
 dpkg-query -W -f='${Package} ${Version}\n' "$app" >> "$qa/previous-versions.txt"
 cp "/usr/bin/$app" "$qa/$app.previous"
done
if [[ -d "$HOME/.config/SoundCurrent" ]]; then cp -a "$HOME/.config/SoundCurrent" "$qa/previous-settings"; fi
sudo -n env DEBIAN_FRONTEND=noninteractive apt-get install -y /tmp/soundcurrent-eq_1.1.0_amd64.deb /tmp/soundcurrent-studio_1.1.0_amd64.deb > "$qa/install.log" 2>&1
for app in soundcurrent-eq soundcurrent-studio; do
 mkdir -p "$qa/$app"
 sha256sum "/tmp/${app}_1.1.0_amd64.deb" > "$qa/$app/package.sha256"
 dpkg-query -W -f='${Package} ${Version} ${Status}\n' "$app" > "$qa/$app/installed.txt"
 for locale in de fr es it pt-PT pt-BR nl pl cs sk uk ru el tr sv da nb fi ro hu nn ar he fa zh-Hans zh-Hant ja ko hi id vi th sw; do
  QT_QPA_PLATFORM=offscreen "/usr/bin/$app" --localization-ui-test --language "$locale" > "$qa/$app/$locale.log" 2>&1
  grep -F "Localization UI: $locale -> $locale" "$qa/$app/$locale.log" >/dev/null
 done
 QT_QPA_PLATFORM=offscreen SOUNDCURRENT_TEST_FORMAT_LOCALE=de-DE "/usr/bin/$app" --localization-ui-test --language ar > "$qa/$app/regional.log" 2>&1
 printf 'PASS: %s in-place DEB upgrade and 33 installed locale fixtures\n' "$app"
done
printf 'Evidence: %s\n' "$qa"
