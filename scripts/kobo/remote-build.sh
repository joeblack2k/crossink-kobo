#!/bin/sh
set -u

repo_dir="$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)"
log_file="${CROSSINK_REMOTE_BUILD_LOG:-$repo_dir/build.log}"
exit_file="${CROSSINK_REMOTE_BUILD_EXIT:-$repo_dir/build.exit}"

rm -f "$exit_file"
"$repo_dir/scripts/kobo/build-rootfs.sh" >"$log_file" 2>&1
code=$?
printf '%s\n' "$code" >"$exit_file"
exit "$code"
