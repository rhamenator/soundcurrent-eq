#!/bin/bash
set -eu
source /home/rich/dev/soundcurrent-eq/scripts/install-linux.run
for tag in fr ar ja; do
 sc_language=$tag
 confirm "$(sc_text download_confirm SoundCurrent 1 Ubuntu)" &
 dialog_pid=$!
 window=''
 for attempt in {1..40}; do
  window=$(xdotool search --onlyvisible --name "$(sc_text install_title)" 2>/dev/null | head -1 || true)
  [[ -z $window ]] || break
  sleep 0.1
 done
 [[ -n $window ]]
 sleep 0.5
 import -window "$window" "/tmp/sc-installer-dialogs/$tag.png"
 xdotool key --window "$window" Escape
 set +e
 wait "$dialog_pid"
 code=$?
 set -e
 [[ $code == 1 ]]
 printf '%s actual GTK cancellation: %s\n' "$tag" "$code"
done
