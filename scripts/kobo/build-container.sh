#!/bin/sh
set -eu

repo_dir="$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)"
image="crossink-kobo-build:bookworm-20260710"
reference_image="${CROSSINK_N437_REFERENCE_IMAGE:-}"
output_dir="${CROSSINK_KOBO_OUTPUT_DIR:-$repo_dir/output-kobo-container}"
output_volume="${CROSSINK_KOBO_DOCKER_VOLUME:-crossink-kobo-build-output-20260711}"
downloads_volume="${CROSSINK_KOBO_DOWNLOADS_VOLUME:-crossink-kobo-build-downloads-20260713}"

[ -n "$reference_image" ] || {
	echo "CROSSINK_N437_REFERENCE_IMAGE must name the full user-owned N437 SD image" >&2
	exit 1
}
case "$reference_image" in
	"$repo_dir"/*) reference_in_container="/workspace/${reference_image#"$repo_dir"/}" ;;
	*)
		echo "Reference image must be located inside $repo_dir for the container build" >&2
		exit 1
		;;
esac
[ -f "$reference_image" ] || {
	echo "Reference image not found: $reference_image" >&2
	exit 1
}
case "$output_dir" in
	"$repo_dir"/*) ;;
	*)
		echo "Container output directory must be located inside $repo_dir" >&2
		exit 1
		;;
esac

command -v docker >/dev/null 2>&1 || {
	echo "Docker CLI ontbreekt; start Colima en installeer de Docker CLI" >&2
	exit 1
}
docker info >/dev/null 2>&1 || {
	echo "Docker-daemon is niet bereikbaar; start Colima" >&2
	exit 1
}

docker build --pull=false -t "$image" -f "$repo_dir/buildroot-external/Dockerfile" "$repo_dir/buildroot-external"
docker volume create "$output_volume" >/dev/null
docker volume create "$downloads_volume" >/dev/null

# The macOS workspace is normally case-insensitive.  The pinned Kobo kernel
# contains both xt_TCPMSS.c and xt_tcpmss.c, so a Git checkout below the bind
# mount silently loses one of them and the kernel cannot build.  Keep
# Buildroot's download/cache tree in a Linux Docker volume instead.  Seed the
# non-kernel downloads once so ordinary package archives are still reused.
docker run --rm \
	-v "$downloads_volume:/downloads" \
	-v "$repo_dir/vendor/buildroot/dl:/seed:ro" \
	"$image" \
	sh -ceu '
		if [ ! -f /downloads/.crossink_seeded_from_workspace ]; then
			rsync -a --exclude="/linux/" /seed/ /downloads/
			touch /downloads/.crossink_seeded_from_workspace
		fi
	'

# Buildroot host packages must chmod files extracted on the bind mount. Docker
# Desktop denies that operation to an arbitrary numeric container user, so
# build as container-root. The final filesystem/image ownership is independently
# fixed and verified under fakeroot by make-image.sh.
docker run --rm \
	-e HOME=/root \
	-e FORCE_UNSAFE_CONFIGURE=1 \
	-e JOBS="${JOBS:-6}" \
	-e CROSSINK_FORCE_APP_REBUILD="${CROSSINK_FORCE_APP_REBUILD:-0}" \
	-e CROSSINK_FORCE_PLATFORM_REBUILD="${CROSSINK_FORCE_PLATFORM_REBUILD:-0}" \
	-e CROSSINK_LINUX_RECONFIGURE="${CROSSINK_LINUX_RECONFIGURE:-0}" \
	-e CROSSINK_N437_REFERENCE_IMAGE="$reference_in_container" \
	-e CROSSINK_KOBO_OUTPUT_DIR=/build-output \
	-v "$output_volume:/build-output" \
	-v "$downloads_volume:/workspace/vendor/buildroot/dl" \
	-v "$repo_dir:/workspace" \
	-w /workspace \
	"$image" \
	./scripts/kobo/build-rootfs.sh

# Export only release inputs. Keeping Buildroot's chmod-sensitive work tree in
# a Docker-managed Linux volume makes repeated builds fast and reliable on
# macOS while the deliverables remain ordinary local files.
mkdir -p "$output_dir"
docker run --rm \
	-v "$output_volume:/build-output:ro" \
	-v "$output_dir:/export" \
	"$image" \
	sh -c 'rm -rf /export/images && mkdir -p /export/images && cp -a /build-output/images/. /export/images/'
