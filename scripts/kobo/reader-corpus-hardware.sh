#!/usr/bin/env bash
# Exercise every user-supplied acceptance EPUB on the physical N437.  This is
# a dev-image harness: it uses only the root-owned FIFO created when
# CROSSINK_DEV_ENABLE is present, never an end-user UI shortcut.
set -euo pipefail

repo_dir=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
target=${CROSSINK_ACCEPTANCE_TARGET:-crossink-n437}
books_dir=${CROSSINK_ACCEPTANCE_BOOKS_DIR:-"$repo_dir/../books"}
device_dir=${CROSSINK_ACCEPTANCE_TARGET_DIR:-/data/Books/Acceptance-Local}
evidence_dir=${CROSSINK_ACCEPTANCE_EVIDENCE_DIR:-"$repo_dir/artifacts/kobo/hardware/reader"}
manifest="$evidence_dir/local-books-manifest.tsv"
results=${CROSSINK_ACCEPTANCE_RESULTS:-"$evidence_dir/n437-reader-corpus.tsv"}
timeout_seconds=${CROSSINK_READER_TIMEOUT_SECONDS:-45}
only_book=${CROSSINK_ACCEPTANCE_ONLY_BOOK:-}

usage() {
  cat >&2 <<EOF
Usage: $(basename "$0") run | run-one <manifest-basename>

Runs open → first render → page forward → in-process re-exec → restored render
for every manifest entry. `run-one` limits the same test to one manifest
basename, which makes a hardware run resumable when the host session ends.
The Kobo must be a dev image with USB SSH reachable.
EOF
  exit 2
}

ensure_manifest() {
  if [[ ! -f "$manifest" ]]; then
    "$repo_dir/scripts/kobo/acceptance-epubs.sh" manifest
  fi
  [[ $(awk 'END { print NR - 1 }' "$manifest") == 12 ]] || {
    echo "Expected 12 manifest books: $manifest" >&2
    exit 1
  }
}

send_dev() {
  local command=$1 encoded
  # Base64 leaves filenames with spaces, apostrophes and non-ASCII bytes out
  # of the remote shell grammar. The command is consumed by the device FIFO.
  encoded=$(printf '%s\n' "$command" | base64 | tr -d '\n')
  ssh -n "$target" "printf '%s' '$encoded' | base64 -d > /run/crossink-dev-input"
}

log_line_count() {
  ssh -n "$target" 'wc -l < /data/.crossink/log/crossink.log 2>/dev/null || echo 0' | tr -d ' '
}

wait_for_render() {
  local path=$1 marker=$2 attempt log
  for ((attempt = 0; attempt < timeout_seconds; ++attempt)); do
    # Only inspect records created by this open/restart.  A global Rendered
    # match can belong to the previous app instance and would make a page
    # command race the new reader activity.
    log=$(ssh -n "$target" "tail -n '+$((marker + 1))' /data/.crossink/log/crossink.log 2>/dev/null || true")
    if printf '%s\n' "$log" | awk -v path="$path" '
      index($0, "Loaded ePub: " path) { loaded = 1 }
      loaded && ($0 ~ /\[ERS\] Rendered page/ || $0 ~ /\[ERS\] No pages to render/) { pass = 1; exit }
      END { exit(pass ? 0 : 1) }
    '; then
      return 0
    fi
    sleep 1
  done
  return 1
}

last_progress() {
  ssh -n "$target" "grep -F 'Progress saved:' /data/.crossink/log/crossink.log | tail -n 1 || true"
}

run() {
  ensure_manifest
  mkdir -p "$evidence_dir"
  local build_id
  build_id=$(ssh -n "$target" 'sha256sum /opt/crossink/current/bin/crossink-kobo | awk "{print \$1}"')
  printf 'build_sha256\tbook\topen\tpage_forward\trestart_restore\twatchdog\tnotes\n' >"$results"

  local name device_path app_path before after status note selected_rows expected_rows
  selected_rows=0
  while IFS=$'\t' read -r name _bytes _hash; do
    [[ $name == basename ]] && continue
    [[ -n $only_book && $name != "$only_book" ]] && continue
    ((selected_rows += 1))
    device_path="${device_dir}/${name}"
    # The Linux filesystem is mounted at /data, while CrossInk's Storage HAL
    # intentionally exposes that mount to activities as /. Never feed the
    # raw Linux mount prefix into the reader activity.
    app_path="${device_path#/data}"
    status=PASS
    note=

    if ! ssh -n "$target" "test -f '$device_path'"; then
      printf '%s\t%s\tFAIL\tFAIL\tFAIL\tFAIL\tmissing on device\n' "$build_id" "$name" >>"$results"
      continue
    fi
    open_marker=$(log_line_count)
    if ! send_dev "open $app_path" || ! wait_for_render "$app_path" "$open_marker"; then
      printf '%s\t%s\tFAIL\tFAIL\tFAIL\tFAIL\topen/render timeout\n' "$build_id" "$name" >>"$results"
      continue
    fi

    before=$(last_progress)
    # This is a reader persistence/render smoke test, not a touch-hitbox
    # test.  A synthetic screen coordinate can legitimately land on an EPUB
    # link, an image or an overlay; use the explicit dev command so every
    # corpus row exercises exactly one page transition.
    send_dev 'page-forward'
    sleep 2
    after=$(last_progress)
    if [[ -z $after || $after == "$before" ]]; then
      # A prior run may have restored this disposable test copy on its final
      # page.  In that case Next is correctly a no-op.  Prove the same
      # forward transition by moving one page back and returning to the
      # original page; never apply this fallback to a normal user book.
      send_dev 'page-back'
      sleep 2
      after_back=$(last_progress)
      returned_to_start=0
      if [[ -n $after_back && $after_back != "$before" ]]; then
        send_dev 'page-forward'
        sleep 2
        after=$(last_progress)
        [[ -n $after && $after == "$before" ]] && returned_to_start=1
      fi
      if (( ! returned_to_start )); then
        status=FAIL
        note='no persisted forward transition (including final-page fallback)'
      fi
    fi

    restart_marker=$(log_line_count)
    if ! send_dev restart || ! wait_for_render "$app_path" "$restart_marker"; then
      printf '%s\t%s\tPASS\t%s\tFAIL\tFAIL\t%s\n' "$build_id" "$name" "$status" "${note:-restart/render timeout}" >>"$results"
      continue
    fi
    if [[ $(ssh -n "$target" 'cat /data/.crossink/watchdog/early-start-failures 2>/dev/null || echo 999') != 0 ]]; then
      status=FAIL
      note="${note:+$note; }watchdog incremented"
    fi
    printf '%s\t%s\tPASS\t%s\tPASS\t%s\t%s\n' "$build_id" "$name" "$status" \
      "$([[ $status == PASS ]] && echo PASS || echo FAIL)" "${note:-ok}" >>"$results"
  done <"$manifest"

  expected_rows=12
  [[ -n $only_book ]] && expected_rows=1
  [[ $selected_rows == "$expected_rows" ]] || {
    echo "Expected $expected_rows selected manifest row(s), found $selected_rows" >&2
    exit 2
  }

  if awk -F '\t' -v expected="$expected_rows" '
      NR > 1 { rows++; if ($3 != "PASS" || $4 != "PASS" || $5 != "PASS" || $6 != "PASS") exit 1 }
      END { exit rows == expected ? 0 : 1 }
    ' "$results"; then
    echo "N437 EPUB acceptance PASS: $results"
  else
    echo "N437 EPUB acceptance FAIL: $results" >&2
    exit 1
  fi
}

case ${1:-} in
  run) run ;;
  run-one)
    [[ $# == 2 ]] || usage
    only_book=$2
    run
    ;;
  *) usage ;;
esac
