#!/bin/sh
set -eu

repo_dir="$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)"
simulator_dir="$repo_dir/vendor/crosspoint-simulator"
patch_file="$repo_dir/platform/kobo/patches/crosspoint-simulator-native-network.patch"
jpegdec_dir="$repo_dir/vendor/JPEGDEC"

apply_git_patch_once() {
	patch_path=$1
	repository=$2

	# A reverse dry-run is the idempotent success case: the exact patch is
	# already present in this reusable vendor checkout.
	if git -C "$repository" apply --reverse --check "$patch_path" 2>/dev/null; then
		return 0
	fi
	git -C "$repository" apply --check "$patch_path"
	git -C "$repository" apply "$patch_path"
}

# Treat the patch as the complete, reviewable native-network delta.  A marker
# check is insufficient: it can hide a partially applied patch and make a
# clean checkout build a different Kobo webserver.
apply_git_patch_once "$patch_file" "$simulator_dir"

# JPEGDEC's pinned progressive path still dereferences the MCU_SKIP sentinel
# on ARM unless these two source patches are applied. The EPUB renderer and
# cover converter both link this same vendored library, so apply them before
# Buildroot configures the Kobo app instead of relying on the ESP-only
# PlatformIO hook in scripts/patch_jpegdec.py.
for jpegdec_patch in "$repo_dir"/scripts/jpegdec_patches/*.patch; do
	[ -f "$jpegdec_patch" ] || continue
	apply_git_patch_once "$jpegdec_patch" "$jpegdec_dir"
done
