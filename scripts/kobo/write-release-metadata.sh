#!/bin/sh
set -eu

usage() {
	echo 'Usage: write-release-metadata.sh --image image --rootfs rootfs.tar --zimage zImage --dtb n437.dtb --manifest output.txt' >&2
	exit 2
}

image=
rootfs=
zimage=
dtb=
manifest=
while [ "$#" -gt 0 ]; do
	case "$1" in
		--image) image=${2:?}; shift 2 ;;
		--rootfs) rootfs=${2:?}; shift 2 ;;
		--zimage) zimage=${2:?}; shift 2 ;;
		--dtb) dtb=${2:?}; shift 2 ;;
		--manifest) manifest=${2:?}; shift 2 ;;
		*) usage ;;
	esac
done
for file in "$image" "$rootfs" "$zimage" "$dtb"; do
	[ -f "$file" ] || { echo "Missing release input: $file" >&2; exit 1; }
done
[ -n "$manifest" ] || usage

repo_dir="$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)"
temporary="$manifest.new"
dirty=no
git -C "$repo_dir" diff --quiet --ignore-submodules -- || dirty=yes
git -C "$repo_dir" diff --cached --quiet --ignore-submodules -- || dirty=yes
[ -z "$(git -C "$repo_dir" ls-files --others --exclude-standard)" ] || dirty=yes

{
	echo 'release=CrossInk-Kobo Beta 1'
	printf 'created_utc='; date -u +%Y-%m-%dT%H:%M:%SZ
	echo "git_head=$(git -C "$repo_dir" rev-parse HEAD)"
	echo "git_branch=$(git -C "$repo_dir" branch --show-current)"
	echo "git_dirty=$dirty"
	echo "sources_lock_sha256=$(sha256sum "$repo_dir/sources.lock" | awk '{print $1}')"
	echo "image_bytes=$(stat -c %s "$image")"
	echo "image_sha256=$(sha256sum "$image" | awk '{print $1}')"
	echo "rootfs_sha256=$(sha256sum "$rootfs" | awk '{print $1}')"
	echo "zimage_sha256=$(sha256sum "$zimage" | awk '{print $1}')"
	echo "dtb_sha256=$(sha256sum "$dtb" | awk '{print $1}')"
	echo 'kernel_branch=kobo/drm-merged-6.19'
	echo 'kernel_commit=ccddaba42e0abafbce84cc2243cbe0c98d400944'
	echo 'target=kobo-glo-hd-n437'
	echo 'waveform_sha256=a158cba8276dc5ed5a146f7465285db1741612a5066497d10269f526a597de67'
} > "$temporary"
mv -f "$temporary" "$manifest"
echo "Wrote $manifest"
