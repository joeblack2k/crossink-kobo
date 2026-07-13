#!/bin/sh
# Run the fixed twelve-EPUB acceptance corpus directly on the N437 dev image.
# This exists because an SSH client can be interrupted by a host UI timeout;
# once launched, the device continues to write a durable result line after
# every book. It is deliberately not installed in a final image.
set -eu

manifest=${1:?usage: run-corpus-on-device.sh MANIFEST RESULTS}
results=${2:?usage: run-corpus-on-device.sh MANIFEST RESULTS}
fifo=/run/crossink-dev-input
log=/data/.crossink/log/crossink.log
book_dir=/Books/Acceptance-Local
timeout_seconds=${CROSSINK_READER_TIMEOUT_SECONDS:-45}
only_book=${CROSSINK_ACCEPTANCE_ONLY_BOOK:-}

[ -p "$fifo" ] || { echo "Missing development FIFO: $fifo" >&2; exit 2; }
[ -r "$manifest" ] || { echo "Missing manifest: $manifest" >&2; exit 2; }

mkdir -p "$(dirname "$results")"
printf 'build_sha256\tbook\topen\tpage_forward\trestart_restore\twatchdog\tnotes\n' >"$results"
build_sha=$(sha256sum /opt/crossink/current/bin/crossink-kobo | awk '{print $1}')

send_dev() {
  printf '%s\n' "$1" >"$fifo"
}

line_count() {
  wc -l <"$log" | tr -d ' '
}

has_since() {
  marker=$1
  needle=$2
  tail -n "+$((marker + 1))" "$log" 2>/dev/null | grep -F "$needle" >/dev/null 2>&1
}

wait_for_line() {
  marker=$1
  needle=$2
  elapsed=0
  while [ "$elapsed" -lt "$timeout_seconds" ]; do
    has_since "$marker" "$needle" && return 0
    sleep 1
    elapsed=$((elapsed + 1))
  done
  return 1
}

wait_for_render() {
  path=$1
  marker=$2
  wait_for_line "$marker" "Loaded ePub: $path" || return 1
  elapsed=0
  while [ "$elapsed" -lt "$timeout_seconds" ]; do
    if has_since "$marker" '[ERS] Rendered page' || has_since "$marker" '[ERS] No pages to render'; then
      return 0
    fi
    sleep 1
    elapsed=$((elapsed + 1))
  done
  return 1
}

wait_for_progress_change() {
  before=$1
  elapsed=0
  while [ "$elapsed" -lt "$timeout_seconds" ]; do
    current=$(grep -F 'Progress saved:' "$log" | tail -n 1 || true)
    [ -n "$current" ] && [ "$current" != "$before" ] && return 0
    sleep 1
    elapsed=$((elapsed + 1))
  done
  return 1
}

append_result() {
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$@" >>"$results"
  sync
}

rows=0
tab=$(printf '\t')
while IFS="$tab" read -r name _bytes _sha; do
  [ "$name" = basename ] && continue
  [ -n "$name" ] || continue
  [ -z "$only_book" ] || [ "$name" = "$only_book" ] || continue
  rows=$((rows + 1))
  path="$book_dir/$name"
  if [ ! -f "/data$path" ]; then
    append_result "$build_sha" "$name" FAIL FAIL FAIL FAIL 'missing on device'
    continue
  fi

  open_marker=$(line_count)
  if ! send_dev "open $path" || ! wait_for_render "$path" "$open_marker"; then
    append_result "$build_sha" "$name" FAIL FAIL FAIL FAIL 'open/render timeout'
    continue
  fi

  before=$(grep -F 'Progress saved:' "$log" | tail -n 1 || true)
  # Exercise the reader's page transition directly.  Coordinate injection is
  # intentionally covered by the separate touch suite: on a document it may
  # activate a link or overlay instead of navigating a page.
  send_dev 'page-forward'
  page_forward=PASS
  if ! wait_for_progress_change "$before"; then
    # A previous acceptance run may have left this disposable copy on its
    # final page.  Verify a real forward transition by stepping back once and
    # returning.  Do not use this harness on a user library path.
    send_dev 'page-back'
    if wait_for_progress_change "$before"; then
      after_back=$(grep -F 'Progress saved:' "$log" | tail -n 1 || true)
      send_dev 'page-forward'
      wait_for_progress_change "$after_back" || page_forward=FAIL
    else
      page_forward=FAIL
    fi
  fi

  restart_restore=PASS
  restart_marker=$(line_count)
  send_dev restart
  if ! wait_for_render "$path" "$restart_marker"; then
    restart_restore=FAIL
  fi
  watchdog=$(cat /data/.crossink/watchdog/early-start-failures 2>/dev/null || echo 999)
  case "$watchdog" in 0) watchdog_result=PASS ;; *) watchdog_result=FAIL ;; esac
  note=ok
  [ "$page_forward" = PASS ] || note='no persisted page-forward result'
  [ "$restart_restore" = PASS ] || note="${note}; restart/render timeout"
  [ "$watchdog_result" = PASS ] || note="${note}; watchdog incremented"
  append_result "$build_sha" "$name" PASS "$page_forward" "$restart_restore" "$watchdog_result" "$note"
done <"$manifest"

expected_rows=12
[ -z "$only_book" ] || expected_rows=1
[ "$rows" -eq "$expected_rows" ] || { echo "Expected $expected_rows manifest rows, got $rows" >&2; exit 2; }
if awk -F '\t' -v expected="$expected_rows" '
    NR > 1 { rows++; if ($3 != "PASS" || $4 != "PASS" || $5 != "PASS" || $6 != "PASS") exit 1 }
    END { exit rows == expected ? 0 : 1 }
  ' "$results"; then
  echo "PASS $results"
else
  echo "FAIL $results" >&2
  exit 1
fi
