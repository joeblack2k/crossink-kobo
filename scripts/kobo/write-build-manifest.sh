#!/bin/sh
# Produce one immutable evidence record for an ARM application build.
set -eu

usage() {
	echo 'Usage: write-build-manifest.sh --binary <crossink-kobo> --buildroot-output <dir> --manifest <file> [--compiler <path>]' >&2
	exit 2
}

binary=
buildroot_output=
manifest=
compiler=
while [ "$#" -gt 0 ]; do
	case "$1" in
		--binary) binary=${2:?}; shift 2 ;;
		--buildroot-output) buildroot_output=${2:?}; shift 2 ;;
		--manifest) manifest=${2:?}; shift 2 ;;
		--compiler) compiler=${2:?}; shift 2 ;;
		*) usage ;;
	esac
done

[ -x "$binary" ] || { echo "Missing executable ARM binary: $binary" >&2; exit 1; }
[ -f "$buildroot_output/.config" ] || { echo "Missing Buildroot config: $buildroot_output/.config" >&2; exit 1; }
[ -n "$manifest" ] || usage
if [ -z "$compiler" ]; then
	compiler="$buildroot_output/host/bin/arm-buildroot-linux-musleabihf-gcc"
fi
[ -x "$compiler" ] || { echo "Missing ARM compiler: $compiler" >&2; exit 1; }
for tool in git sha256sum file awk date dirname mkdir mv; do
	command -v "$tool" >/dev/null 2>&1 || { echo "Missing required tool: $tool" >&2; exit 1; }
done

repo_dir=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
mkdir -p "$(dirname "$manifest")"
temporary="$manifest.new"

# Include the index, unstaged changes and untracked path list.  A dirty tree
# remains acceptable for development evidence, but never gets mistaken for a
# release-clean build.
dirty_payload=$(mktemp "${TMPDIR:-/tmp}/crossink-dirty.XXXXXX")
cleanup() { rm -f "$dirty_payload" "$temporary"; }
trap cleanup EXIT HUP INT TERM
{
	git -C "$repo_dir" diff --binary
	git -C "$repo_dir" diff --cached --binary
	git -C "$repo_dir" status --porcelain=v1 --untracked-files=all
} >"$dirty_payload"

{
	echo 'record_type=crossink-kobo-build'
	printf 'created_utc='; date -u +%Y-%m-%dT%H:%M:%SZ
	echo "git_head=$(git -C "$repo_dir" rev-parse HEAD)"
	echo "git_branch=$(git -C "$repo_dir" branch --show-current)"
	echo "dirty_fingerprint_sha256=$(sha256sum "$dirty_payload" | awk '{print $1}')"
	echo "sources_lock_sha256=$(sha256sum "$repo_dir/sources.lock" | awk '{print $1}')"
	echo "buildroot_config_sha256=$(sha256sum "$buildroot_output/.config" | awk '{print $1}')"
	echo "binary_path=$binary"
	echo "binary_sha256=$(sha256sum "$binary" | awk '{print $1}')"
	echo "binary_file=$(file -b "$binary")"
	echo "compiler=$($compiler --version 2>/dev/null | awk 'NR == 1 {print; exit}')"
	echo "target=kobo-glo-hd-n437"
} >"$temporary"
mv -f "$temporary" "$manifest"
trap - EXIT HUP INT TERM
rm -f "$dirty_payload"
echo "Wrote $manifest"
