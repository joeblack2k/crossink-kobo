#!/bin/sh
set -eu

usage() {
	echo "Usage: test-grow-data-image.sh --image crossink.img --size-bytes <larger-card-size>" >&2
	exit 2
}

image=
size_bytes=
while [ "$#" -gt 0 ]; do
	case "$1" in
		--image) image=${2:?}; shift 2 ;;
		--size-bytes) size_bytes=${2:?}; shift 2 ;;
		*) usage ;;
	esac
done
[ -f "$image" ] || usage
case "$size_bytes" in ''|*[!0-9]*) usage ;; esac
[ "$(id -u)" -eq 0 ] || { echo "Run this test as root in Linux" >&2; exit 1; }
for tool in awk blockdev cmp cp dd dumpe2fs losetup mknod mount mountpoint partx resize2fs sed sfdisk stat truncate umount; do
	command -v "$tool" >/dev/null 2>&1 || { echo "Missing tool: $tool" >&2; exit 1; }
done

repo_dir=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
grow_script="$repo_dir/buildroot-external/board/kobo-glo-hd/rootfs-overlay/usr/sbin/crossink-grow-data"
work=$(mktemp -d "${TMPDIR:-/tmp}/crossink-grow-image.XXXXXX")
loop=
create_partition_nodes() {
	loop_name=$(basename "$loop")
	[ -r "/sys/class/block/$loop_name/${loop_name}p4/dev" ] || partx -a "$loop"
	for partition in 1 2 3 4; do
		device_numbers=$(cat "/sys/class/block/$loop_name/${loop_name}p$partition/dev")
		rm -f "${loop}p$partition"
		mknod "${loop}p$partition" b "${device_numbers%:*}" "${device_numbers#*:}"
	done
}
cleanup() {
	mountpoint -q /mnt/crossink-user && umount /mnt/crossink-user || true
	mountpoint -q /boot && umount /boot || true
	[ -z "$loop" ] || losetup -d "$loop" 2>/dev/null || true
	rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM

copy="$work/card.img"
cp --sparse=always "$image" "$copy"
[ "$size_bytes" -gt "$(stat -c %s "$copy")" ] || {
	echo "Test card must be larger than the release image" >&2
	exit 1
}
truncate -s "$size_bytes" "$copy"
dd if="$copy" of="$work/original.mbr" bs=512 count=1 status=none

loop=$(losetup --find --show --partscan "$copy")
if [ ! -b "${loop}p4" ]; then
	create_partition_nodes
fi
for _ in 1 2 3 4 5; do
	[ -b "${loop}p4" ] && break
	sleep 1
done
[ -b "${loop}p4" ] || { echo "Loop partitions were not created" >&2; exit 1; }
fixed_before=$(sfdisk -d "$loop" | sed -n 's/.*p\([1-3]\) : start= *\([0-9]*\), size= *\([0-9]*\), type= *\([^, ]*\).*/\1,\2,\3,\4/p')

# Exercise the production script against a loop device and replace only the
# hardware model read and reboot action in this disposable test copy.
test_script="$work/crossink-grow-data"
sed \
	-e "s#^DISK=/dev/mmcblk0\$#DISK=$loop#" \
	-e "s#/dev/mmcblk0p#${loop}p#g" \
	-e 's#cat /proc/device-tree/model 2>/dev/null || true#printf "%s" "Kobo Glo HD N437 (CrossInk)"#' \
	-e 's#reboot -f#: > /tmp/crossink-grow-reboot-requested#' \
	"$grow_script" > "$test_script"
chmod +x "$test_script"

mkdir -p /boot /mnt/crossink-user
mount "${loop}p1" /boot
rm -f /tmp/crossink-grow-reboot-requested
if "$test_script"; then
	echo "Partition phase did not request a reboot" >&2
	exit 1
fi
[ -e /tmp/crossink-grow-reboot-requested ] || {
	echo "Partition phase did not reach the reboot boundary" >&2
	exit 1
}
[ -e /boot/flags/CROSSINK_DATA_PARTITIONED ] || {
	echo "Partition marker is missing" >&2
	exit 1
}
cmp -s "$work/original.mbr" /boot/crossink-mbr.before-grow.bin || {
	echo "Original MBR backup mismatch" >&2
	exit 1
}

umount /boot
blockdev --rereadpt "$loop"
create_partition_nodes
mount "${loop}p1" /boot
"$test_script"
[ -e /boot/flags/CROSSINK_DATA_GROWN ] || { echo "Growth marker is missing" >&2; exit 1; }

fixed_after=$(sfdisk -d "$loop" | sed -n 's/.*p\([1-3]\) : start= *\([0-9]*\), size= *\([0-9]*\), type= *\([^, ]*\).*/\1,\2,\3,\4/p')
[ "$fixed_before" = "$fixed_after" ] || { echo "p1-p3 changed during growth" >&2; exit 1; }
partition_sectors=$(sfdisk -d "$loop" | sed -n 's/.*p4 : start= *[0-9]*, size= *\([0-9]*\).*/\1/p')
expected_sectors=$((size_bytes / 512 - 1415168))
[ "$partition_sectors" = "$expected_sectors" ] || { echo "p4 does not end at card boundary" >&2; exit 1; }

filesystem_sectors=$(dumpe2fs -h "${loop}p4" 2>/dev/null | awk -F: '
	/Block count:/ { count=$2 + 0 }
	/Block size:/ { size=$2 + 0 }
	END { print count * size / 512 }
')
[ "$filesystem_sectors" = "$partition_sectors" ] || { echo "ext4 did not fill p4" >&2; exit 1; }
mount "${loop}p4" /mnt/crossink-user
[ -d /mnt/crossink-user/Books ] || { echo "/Books is missing from p4" >&2; exit 1; }
umount /mnt/crossink-user

# A completed image must be a no-op on subsequent boots.
rm -f /tmp/crossink-grow-reboot-requested
"$test_script"
[ ! -e /tmp/crossink-grow-reboot-requested ] || { echo "Growth repeated after completion" >&2; exit 1; }
echo "CrossInk p4 growth on a $size_bytes-byte card: OK"
