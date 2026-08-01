#!/usr/bin/env bash
# Capture the same Kobo Home and Settings surfaces at every supported UI scale.
# This is a dev-image acceptance helper: it requires CROSSINK_DEV_ENABLE and
# restores the previous persisted scale even when a capture fails.
set -euo pipefail

host=${CROSSINK_SSH_HOST:-crossink-n437}
destination=${1:-"artifacts/kobo/hardware/scale/$(date -u +%Y%m%dT%H%M%SZ)"}
mkdir -p "$destination"

remote_settings=/data/.crosspoint/crossink-settings.json
remote_fifo=/run/crossink-dev-input
remote_shot=/data/.crossink/screenshots/live.pbm

ssh "$host" "test -e /boot/flags/CROSSINK_DEV_ENABLE && test -p $remote_fifo && test -f $remote_settings"
original=$(ssh "$host" "sed -n 's/.*\"koboUiScalePercent\":\([0-9][0-9]*\).*/\1/p' $remote_settings")
case "$original" in 100|150|200|250) ;; *) original=200 ;; esac

restore() {
  ssh "$host" "sed -i 's/\"koboUiScalePercent\":[0-9][0-9]*/\"koboUiScalePercent\":$original/' $remote_settings" || true
  printf 'restart\n' | base64 | tr -d '\n' | ssh "$host" "base64 -d > $remote_fifo" || true
}
trap restore EXIT

send() {
  printf '%s\n' "$1" | base64 | tr -d '\n' | ssh "$host" "base64 -d > $remote_fifo"
}

capture() {
  local scale=$1 screen=$2
  send "$screen"
  sleep 1
  send screenshot
  sleep 1
  ssh "$host" "cat $remote_shot" > "$destination/${scale}-${screen}.pbm"
  sips -s format png "$destination/${scale}-${screen}.pbm" --out "$destination/${scale}-${screen}.png" >/dev/null
  sips -r 90 "$destination/${scale}-${screen}.png" --out "$destination/${scale}-${screen}-portrait.png" >/dev/null
}

for scale in 100 150 200 250; do
  ssh "$host" "sed -i 's/\"koboUiScalePercent\":[0-9][0-9]*/\"koboUiScalePercent\":$scale/' $remote_settings"
  send restart
  sleep 3
  capture "$scale" home
  capture "$scale" settings
done

printf 'previous_scale=%s\nscales=100,150,200,250\n' "$original" > "$destination/manifest.txt"
echo "Captured $destination"
