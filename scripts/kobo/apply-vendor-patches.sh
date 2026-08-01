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
	if git -C "$repository" apply --check "$patch_path"; then
		git -C "$repository" apply "$patch_path"
		return 0
	fi

	# The Kobo source tree deliberately extends the patched native-network
	# files.  Once those later changes touch patch context, neither a forward
	# nor reverse dry-run can succeed although the complete Kobo delta is
	# already present.  Do not reapply the old vendor patch over that worktree;
	# fail only when its Kobo marker is genuinely absent.
	case "$(basename "$patch_path")" in
		crosspoint-simulator-native-network.patch)
			grep -q 'KoboSystemCompat' "$repository/src/Arduino.h" && \
			grep -q 'KoboPortalHtml.generated.h' "$repository/src/CrossPointWebServer.cpp" && return 0
			;;
	esac
	echo "Required vendor patch is neither applicable nor present: $patch_path" >&2
	return 1
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
