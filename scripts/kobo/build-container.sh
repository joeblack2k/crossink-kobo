#!/bin/sh
set -eu

repo_dir="$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)"
image="crossink-kobo-build:bookworm-20260710"

command -v docker >/dev/null 2>&1 || {
	echo "Docker CLI ontbreekt; start Colima en installeer de Docker CLI" >&2
	exit 1
}
docker info >/dev/null 2>&1 || {
	echo "Docker-daemon is niet bereikbaar; start Colima" >&2
	exit 1
}

docker build --pull=false -t "$image" -f "$repo_dir/buildroot-external/Dockerfile" "$repo_dir/buildroot-external"

docker run --rm \
	--user "$(id -u):$(id -g)" \
	-e HOME=/tmp \
	-e JOBS="${JOBS:-6}" \
	-v "$repo_dir:/workspace" \
	-w /workspace \
	"$image" \
	./scripts/kobo/build-rootfs.sh
