#!/usr/bin/env bash
# Roll back only the application symlink. Kernel, rootfs and userdata remain
# untouched, so this command is safe for the development card.
set -euo pipefail

host=${1:-crossink-n437}
ssh -o BatchMode=yes "$host" '
  set -eu
  current=/opt/crossink/current
  test -L "$current" || { echo "No active CrossInk release" >&2; exit 1; }
  active=$(readlink "$current")
  previous=$(sed -n "1p" "$active/previous-release.txt" 2>/dev/null || true)
  case "$previous" in ""|none|broken) echo "No rollback release recorded" >&2; exit 1 ;; esac
  test -x "$previous/bin/crossink-kobo" || { echo "Rollback binary missing: $previous" >&2; exit 1; }
  ln -s "$previous" /opt/crossink/current.rollback
  # BusyBox follows a directory symlink unless -T is supplied.  Keep the
  # rollback switch as atomic as the deployment activation switch.
  mv -fT /opt/crossink/current.rollback "$current"
  /etc/init.d/S60crossink restart
  sha256sum "$current/bin/crossink-kobo"
'
