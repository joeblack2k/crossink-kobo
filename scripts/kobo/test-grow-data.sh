#!/bin/sh
set -eu
script=$(CDPATH='' cd -- "$(dirname "$0")/../../" && pwd)/buildroot-external/board/kobo-glo-hd/rootfs-overlay/usr/sbin/crossink-grow-data
init=$(CDPATH='' cd -- "$(dirname "$0")/../../" && pwd)/buildroot-external/board/kobo-glo-hd/rootfs-overlay/etc/init.d/S10crossink-data
defconfig=$(CDPATH='' cd -- "$(dirname "$0")/../../" && pwd)/buildroot-external/configs/crossink_kobo_glo_hd_defconfig
work=$(mktemp -d "${TMPDIR:-/tmp}/crossink-grow-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
sh -n "$script"
sh -n "$init"
grep -q 'DISK=/dev/mmcblk0' "$script"
grep -q '1415168' "$script"
grep -q 'label-id: 0x4370b001' "$script"
grep -q 'resize2fs' "$script"
grep -q 'reboot -f' "$script"
! grep -q 'CROSSINK_DEV_ENABLE' "$script"
grep -q '1415168,\$desired_size,83' "$script"
grep -q 'sfdisk .* -N 4 ' "$script"
grep -q 'refusing to shrink' "$script"
grep -q 'not mounted from' "$script"
grep -q 'leaving /data unmounted' "$init"
grep -q 'BR2_PACKAGE_E2FSPROGS_RESIZE2FS=y' "$defconfig"
grep -q 'BR2_PACKAGE_UTIL_LINUX_BINARIES=y' "$defconfig"

# Exercise the actual marker helper with failing storage primitives.
eval "$(sed -n '/^fail()/,/^\[ -e "\$COMPLETE"/p' "$script" | sed '$d')"
mkdir "$work/fake-bin"
printf '%s\n' '#!/bin/sh' 'exit 1' > "$work/fake-bin/sync"
chmod +x "$work/fake-bin/sync"
if (PATH="$work/fake-bin:$PATH"; persist_marker "$work/marker"); then
	echo "marker sync failure was accepted" >&2
	exit 1
fi
if persist_marker "$work"; then
	echo "marker write failure was accepted" >&2
	exit 1
fi

# A failed grow helper must stop S10 before it touches /data.
printf '%s\n' '#!/bin/sh' 'exit 1' > "$work/fail-grow"
printf '%s\n' '#!/bin/sh' 'printf "%s\n" "$*" >> "$CALL_LOG"' 'exit 0' > "$work/fake-bin/mountpoint"
printf '%s\n' '#!/bin/sh' 'exit 0' > "$work/fake-bin/mkdir"
chmod +x "$work/fail-grow" "$work/fake-bin/mountpoint" "$work/fake-bin/mkdir"
CALL_LOG="$work/mountpoint-calls" CROSSINK_GROW_HELPER="$work/fail-grow" \
	PATH="$work/fake-bin:$PATH" sh "$init" start
if grep -q '/data' "$work/mountpoint-calls"; then
	echo "/data was inspected after grow failure" >&2
	exit 1
fi
echo "crossink-grow-data syntax and guard checks: OK"
