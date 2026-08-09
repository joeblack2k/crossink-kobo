#!/bin/sh
# Assemble a CrossInk-only N437 image on Linux.
set -eu

# Non-login SSH sessions on Debian omit the administration directories, while
# sfdisk, losetup and mkfs.ext4 intentionally live there.
PATH=/usr/sbin:/sbin:$PATH
export PATH

# rootfs.tar contains the ownership produced by Buildroot's fakeroot stage.
# Extracting it as an ordinary build user would silently rewrite every inode
# to that user's uid/gid, which makes Dropbear reject /root/.ssh and can break
# other daemons. Re-exec the complete filesystem assembly inside fakeroot so
# tar, stat and mkfs.ext4 all observe the original numeric ownership.
if [ "$(id -u)" -ne 0 ]; then
	command -v fakeroot >/dev/null 2>&1 || {
		echo 'fakeroot is required for unprivileged image assembly' >&2
		exit 1
	}
	exec fakeroot -- "$0" "$@"
fi

usage() {
	printf '%s\n' \
		'Usage: make-image.sh --reference reference.img --zimage zImage --dtb n437.dtb --rootfs rootfs.tar --recovery recovery.tar --output crossink.img --size-bytes <exact-card-size>' \
		'Add --dev-enable only for an explicitly requested development image.' \
		'Requires Linux, sfdisk, mkfs.ext4 and mkimage; no loop devices are used.' >&2
	exit 2
}

reference='' zimage='' dtb='' rootfs='' recovery='' output='' size_bytes='' dev_enable=0
while [ "$#" -gt 0 ]; do
	case "$1" in
		--reference) reference=${2:?}; shift 2 ;;
		--zimage) zimage=${2:?}; shift 2 ;;
		--dtb) dtb=${2:?}; shift 2 ;;
		--rootfs) rootfs=${2:?}; shift 2 ;;
		--recovery) recovery=${2:?}; shift 2 ;;
		--output) output=${2:?}; shift 2 ;;
		--size-bytes) size_bytes=${2:?}; shift 2 ;;
		--dev-enable) dev_enable=1; shift ;;
		-h|--help) usage ;;
		*) echo "Unknown option: $1" >&2; usage ;;
	esac
done

for tool in cat cmp dd id sfdisk mkfs.ext4 mkimage tar sha256sum tail truncate touch wc; do
	command -v "$tool" >/dev/null 2>&1 || { echo "Missing tool: $tool" >&2; exit 1; }
done
for input in "$reference" "$zimage" "$dtb" "$rootfs" "$recovery"; do
	[ -f "$input" ] || { echo "Missing input: $input" >&2; exit 1; }
done
case "$size_bytes" in ''|*[!0-9]*) echo '--size-bytes must be decimal' >&2; exit 2 ;; esac
[ $((size_bytes % 512)) -eq 0 ] || { echo 'Card size must be divisible by 512' >&2; exit 1; }
# The system partitions have a fixed footprint; only the user partition grows
# with the supplied card.  Do not impose an arbitrary 2 GiB binary boundary:
# many cards marketed as "2 GB" are slightly below that value.  Require the
# actual minimum instead: enough room for the fixed partition layout plus a
# 512 MiB writable library/cache partition.
minimum_user_sectors=1048576
source_date_epoch=${SOURCE_DATE_EPOCH:-1783690441}
case "$source_date_epoch" in ''|*[!0-9]*) echo 'SOURCE_DATE_EPOCH must be decimal' >&2; exit 1 ;; esac
export SOURCE_DATE_EPOCH="$source_date_epoch" E2FSPROGS_FAKE_TIME="$source_date_epoch"

# Measured N437 legacy-loader geometry. p2 begins after the fixed raw uImage slot.
sector_size=512
boot_start=30720
boot_size=49152
kernel_start=81920
legacy_kernel_start=2048
legacy_kernel_size=12284
recovery_start=104448
recovery_size=262144
root_start=$((recovery_start + recovery_size))
root_size=1048576
user_start=$((root_start + root_size))
total_sectors=$((size_bytes / sector_size))
user_size=$((total_sectors - user_start))
kernel_room=$(((recovery_start - kernel_start) * sector_size))
[ "$user_size" -ge "$minimum_user_sectors" ] || {
	echo "Insufficient user-partition space: need at least $((minimum_user_sectors * sector_size)) bytes" >&2
	exit 1
}

output_dir=$(CDPATH='' cd -- "$(dirname "$output")" && pwd)
output="$output_dir/$(basename "$output")"
[ ! -e "$output" ] || { echo "Refusing existing output: $output" >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/crossink-n437-image.XXXXXX")
cleanup() {
	rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM

boot_stage="$work/boot"
root_stage="$work/root"
recovery_stage="$work/recovery"
user_stage="$work/user"
mkdir -p "$boot_stage/flags" "$root_stage" "$recovery_stage" \
	"$user_stage/Books" "$user_stage/.crossink/crash" "$user_stage/.crossink/log"

safe_extract() {
	archive=$1
	destination=$2
	if ! tar -tf "$archive" | awk 'index($0,"/")==1 || $0==".." || index($0,"../")==1 || index($0,"/../") { exit 1 }'; then
		echo "Unsafe archive member: $archive" >&2
		exit 1
	fi
	tar --numeric-owner -xf "$archive" -C "$destination"
}
safe_extract "$rootfs" "$root_stage"
safe_extract "$recovery" "$recovery_stage"

# CONFIG_ARM_APPENDED_DTB makes the kernel consume a DTB concatenated to the
# compressed image.  Do this explicitly instead of relying on Buildroot's
# hidden BR2_LINUX_KERNEL_APPENDED_DTB symbol: the raw N437 loader only loads
# one uImage and has no separate DTB command.
kernel_payload="$work/zImage-n437-appended-dtb"
cat "$zimage" "$dtb" > "$kernel_payload"
[ "$(( $(wc -c < "$kernel_payload") + 64 ))" -le "$kernel_room" ] || {
	echo 'zImage plus N437 DTB exceeds raw loader slot' >&2
	exit 1
}
tail -c "$(wc -c < "$dtb")" "$kernel_payload" | cmp -s - "$dtb" || {
	echo 'Failed to append N437 DTB to zImage' >&2
	exit 1
}

# p1 and p2 are wholly CrossInk-owned: no InkBox filesystem is retained.
[ "$dev_enable" -eq 0 ] || : > "$boot_stage/flags/CROSSINK_DEV_ENABLE"
: > "$boot_stage/flags/CROSSINK_RECOVERY_AVAILABLE"
printf 'CrossInk Kobo Beta 3\n' > "$boot_stage/README"
mkdir -p "$recovery_stage/etc"
printf 'minimal recovery\n' > "$recovery_stage/etc/crossink-mode"
if find "$root_stage" "$recovery_stage" -type f \( -name 'id_*' -o -name 'pass.txt' \) -print -quit | grep -q .; then
	echo 'Refusing private key or pass.txt in image' >&2
	exit 1
fi
if grep -R -I -E '^[[:space:]]*(ssid|psk|password)=' "$root_stage/etc" "$recovery_stage/etc" 2>/dev/null | grep -q .; then
	echo 'Refusing populated Wi-Fi credentials in image' >&2
	exit 1
fi
find "$boot_stage" "$root_stage" "$recovery_stage" "$user_stage" -exec touch -h -d "@$source_date_epoch" {} +

# Preserve only the raw pre-p1 boot area. Its MBR is replaced below; reference
# partitions are neither copied nor mounted.
truncate -s "$size_bytes" "$output"
dd if="$reference" of="$output" bs=512 count="$boot_start" conv=notrunc status=none
# The reference's normal-era kernel at sector 2048 is not used by the InkBox
# N437 loader (load_ntxkernel reads sector 81920), and retaining it would leave
# unrelated executable code in our image.  Keep HWCFG and the panel waveform,
# but explicitly sanitize that obsolete raw slot.
dd if=/dev/zero of="$output" bs=512 seek="$legacy_kernel_start" \
	count="$legacy_kernel_size" conv=notrunc status=none
sfdisk --no-reread --no-tell-kernel "$output" <<EOF
label: dos
label-id: 0x4370b001
unit: sectors

${boot_start},${boot_size},83
${recovery_start},${recovery_size},83
${root_start},${root_size},83
${user_start},${user_size},83
EOF

mkfs_ext4() {
	uuid=$1 label=$2 stage=$3 image=$4 sectors=$5
	truncate -s "$((sectors * sector_size))" "$image"
	mkfs.ext4 -q -F -O '^64bit' -E lazy_itable_init=0,lazy_journal_init=0 \
		-U "$uuid" -L "$label" -d "$stage" "$image"
}
boot_image="$work/p1.ext4"
recovery_image="$work/p2.ext4"
root_image="$work/p3.ext4"
user_image="$work/p4.ext4"
mkfs_ext4 43700001-0000-4000-8000-000000000001 crossink-boot "$boot_stage" "$boot_image" "$boot_size"
mkfs_ext4 43700002-0000-4000-8000-000000000002 crossink-recov "$recovery_stage" "$recovery_image" "$recovery_size"
mkfs_ext4 43700003-0000-4000-8000-000000000003 crossink-root "$root_stage" "$root_image" "$root_size"
mkfs_ext4 43700004-0000-4000-8000-000000000004 crossink-user "$user_stage" "$user_image" "$user_size"

# Writing partition filesystem files by offset avoids privileged loop devices
# and works inside an unprivileged LXC. conv=sparse preserves empty user space.
dd if="$boot_image" of="$output" bs=512 seek="$boot_start" conv=notrunc,sparse status=none
dd if="$recovery_image" of="$output" bs=512 seek="$recovery_start" conv=notrunc,sparse status=none
dd if="$root_image" of="$output" bs=512 seek="$root_start" conv=notrunc,sparse status=none
dd if="$user_image" of="$output" bs=512 seek="$user_start" conv=notrunc,sparse status=none

uimage="$work/uImage"
mkimage -A arm -O linux -T kernel -C none -a 0x10008000 -e 0x10008000 \
	-n 'CrossInk Kobo N437 6.19' -d "$kernel_payload" "$uimage" >/dev/null
dd if="$uimage" of="$output" bs=512 seek="$kernel_start" conv=notrunc status=none
sync

manifest="$output.manifest"
{
	echo "image=$(basename "$output")"
	echo "image_bytes=$size_bytes"
	echo "source_date_epoch=$source_date_epoch"
	echo "reference_sha256=$(sha256sum "$reference" | awk '{print $1}')"
	echo "zimage_sha256=$(sha256sum "$zimage" | awk '{print $1}')"
	echo "dtb_sha256=$(sha256sum "$dtb" | awk '{print $1}')"
	echo "kernel_payload_sha256=$(sha256sum "$kernel_payload" | awk '{print $1}')"
	echo "rootfs_sha256=$(sha256sum "$rootfs" | awk '{print $1}')"
	echo "recovery_sha256=$(sha256sum "$recovery" | awk '{print $1}')"
	echo "image_sha256=$(sha256sum "$output" | awk '{print $1}')"
	echo "device_data_source_sectors=1-$((boot_start - 1))"
	echo "sanitized_legacy_kernel_sectors=$legacy_kernel_start-$((legacy_kernel_start + legacy_kernel_size - 1))"
	echo 'waveform_header_sector=14335'
	echo 'waveform_payload_sector=14336'
	echo "raw_kernel_sector=$kernel_start"
	echo 'root_partition=/dev/mmcblk0p3'
} > "$manifest"
echo "Created $output"
echo "Manifest $manifest"
