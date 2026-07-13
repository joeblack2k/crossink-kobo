#!/usr/bin/env bash
# Atomically deploy one already-built CrossInk binary to a development Kobo.
# This intentionally never writes a boot or rootfs partition: only the
# versioned application slot used by crossink-supervisor is touched.
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: deploy-dev.sh --binary PATH --manifest PATH [--host SSH_ALIAS] [--version VERSION]

Installs PATH as /opt/crossink/releases/VERSION/bin/crossink-kobo, atomically
switches /opt/crossink/current, restarts the supervisor, and verifies the
deployed SHA-256. The previous symlink target remains intact for rollback.
EOF
  exit 2
}

binary=
manifest=
host=crossink-n437
version=
while (($#)); do
  case "$1" in
    --binary) binary=${2:?missing value}; shift 2 ;;
    --manifest) manifest=${2:?missing value}; shift 2 ;;
    --host) host=${2:?missing value}; shift 2 ;;
    --version) version=${2:?missing value}; shift 2 ;;
    *) usage ;;
  esac
done

[[ -n "$binary" && -n "$manifest" ]] || usage
[[ -f "$binary" && -x "$binary" ]] || { echo "Missing executable: $binary" >&2; exit 1; }
[[ -f "$manifest" ]] || { echo "Missing build manifest: $manifest" >&2; exit 1; }

local_sha=$(shasum -a 256 "$binary" | awk '{print $1}')
if [[ -z "$version" ]]; then
  version="dev-${local_sha:0:12}"
fi
[[ "$version" =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid release version' >&2; exit 2; }

remote_stage="/tmp/crossink-deploy-${version}-$$"

cleanup_remote() {
  ssh -o BatchMode=yes "$host" "rm -rf '$remote_stage'" >/dev/null 2>&1 || true
}
trap cleanup_remote EXIT

# Confirm the expected physical model before transferring data. This also
# makes a disconnected USB-gadget link fail without changing the device.
ssh -o BatchMode=yes "$host" '
  set -eu
  model=$(tr -d "\000" </proc/device-tree/model 2>/dev/null || true)
  case "$model" in *"Kobo Glo HD"*|*"N437"*) ;; *) echo "Refusing non-N437 target: $model" >&2; exit 65 ;; esac
  test -x /usr/sbin/crossink-supervisor
  mkdir -p /opt/crossink/releases
'

tar -C "$(dirname "$binary")" -cf - "$(basename "$binary")" |
  ssh -o BatchMode=yes "$host" "set -eu; umask 077; mkdir -p '$remote_stage'; tar -C '$remote_stage' -xf -"
tar -C "$(dirname "$manifest")" -cf - "$(basename "$manifest")" |
  ssh -o BatchMode=yes "$host" "set -eu; tar -C '$remote_stage' -xf -"

remote_sha=$(ssh -o BatchMode=yes "$host" "sha256sum '$remote_stage/$(basename "$binary")' | awk '{print \$1}'")
[[ "$remote_sha" == "$local_sha" ]] || { echo "Transfer checksum mismatch: $remote_sha != $local_sha" >&2; exit 1; }

ssh -o BatchMode=yes "$host" "
  set -eu
  release='/opt/crossink/releases/$version'
  stage='$remote_stage'
  test ! -e \"\$release\" || { echo \"Release already exists: \$release\" >&2; exit 73; }
  mkdir -p \"\$release/bin\"
  install -m 0755 \"\$stage/$(basename "$binary")\" \"\$release/bin/crossink-kobo\"
  install -m 0644 \"\$stage/$(basename "$manifest")\" \"\$release/build-manifest.txt\"
  previous=none
  if [ -L /opt/crossink/current ]; then previous=\$(readlink /opt/crossink/current || printf broken); fi
  printf '%s\\n' \"\$previous\" > \"\$release/previous-release.txt\"
  sync
  ln -s \"\$release\" /opt/crossink/current.new
  # BusyBox follows a symlink to a directory unless -T is supplied. Without
  # it, the staged link is moved into the old release instead of replacing
  # /opt/crossink/current.
  mv -fT /opt/crossink/current.new /opt/crossink/current
  /etc/init.d/S60crossink restart
"

sleep 3
deployed_sha=$(ssh -o BatchMode=yes "$host" "sha256sum /opt/crossink/current/bin/crossink-kobo | awk '{print \$1}'")
[[ "$deployed_sha" == "$local_sha" ]] || {
  echo "Deployment checksum mismatch after activation: $deployed_sha != $local_sha" >&2
  exit 1
}
ssh -o BatchMode=yes "$host" "printf 'deployed=%s\\nsha256=%s\\n' \"\$(readlink /opt/crossink/current)\" '$deployed_sha'"
