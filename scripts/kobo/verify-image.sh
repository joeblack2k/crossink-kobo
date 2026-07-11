#!/bin/sh
# Read-only structural validation for a CrossInk N437 image.
set -eu
PATH=/usr/sbin:/sbin:$PATH
export PATH

usage() {
	echo 'Usage: verify-image.sh --image crossink.img --dtb n437.dtb --size-bytes <exact-size>' >&2
	exit 2
}

image=
dtb=
size_bytes=
while [ "$#" -gt 0 ]; do
	case "$1" in
		--image) image=${2:?}; shift 2 ;;
		--dtb) dtb=${2:?}; shift 2 ;;
		--size-bytes) size_bytes=${2:?}; shift 2 ;;
		-h|--help) usage ;;
		*) usage ;;
	esac
done
[ -f "$image" ] || { echo "Missing image: $image" >&2; exit 1; }
[ -f "$dtb" ] || { echo "Missing N437 DTB: $dtb" >&2; exit 1; }
case "$size_bytes" in ''|*[!0-9]*) usage ;; esac
for tool in cmp sfdisk e2label debugfs dumpimage dd stat mktemp sha256sum file grep find tail wc; do
	command -v "$tool" >/dev/null 2>&1 || { echo "Missing tool: $tool" >&2; exit 1; }
done

actual_size=$(stat -c %s "$image")
[ "$actual_size" = "$size_bytes" ] || {
	echo "Image size mismatch: expected $size_bytes, got $actual_size" >&2
	exit 1
}

user_size=$((size_bytes / 512 - 1415168))
expected_table="30720,49152,83
104448,262144,83
366592,1048576,83
1415168,${user_size},83"
actual_table=$(sfdisk -d "$image" | sed -n 's/.*start= *\([0-9][0-9]*\), size= *\([0-9][0-9]*\), type= *\([0-9A-Fa-f][0-9A-Fa-f]*\).*/\1,\2,\3/p' | sed -n '1,4p')
[ "$actual_table" = "$expected_table" ] || {
	echo 'Unexpected CrossInk N437 p1-p3 geometry:' >&2
	printf '%s\n' "$actual_table" >&2
	exit 1
}

work=$(mktemp -d "${TMPDIR:-/tmp}/crossink-n437-verify.XXXXXX")
cleanup() {
	rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM

# The loader slot is a legacy ARM uImage at the measured 40 MiB offset.
dd if="$image" of="$work/uImage" bs=512 skip=81920 count=22528 status=none
header=$(dumpimage -l "$work/uImage")
# U-Boot releases use either a dedicated `Architecture:` line or include the
# architecture in `Image Type:`. Accept both representations while still
# requiring ARM explicitly.
printf '%s\n' "$header" | grep -Eq 'Architecture:[[:space:]]+ARM|Image Type:[[:space:]]+ARM ' || {
	echo 'uImage is not ARM' >&2
	exit 1
}
printf '%s\n' "$header" | grep -q 'Load Address: 10008000' || { echo 'uImage load address is wrong' >&2; exit 1; }
printf '%s\n' "$header" | grep -q 'Entry Point:  10008000' || { echo 'uImage entry address is wrong' >&2; exit 1; }
dumpimage -T kernel -p 0 -o "$work/kernel-payload" "$work/uImage" >/dev/null
dtb_size=$(wc -c < "$dtb")
[ "$(wc -c < "$work/kernel-payload")" -gt "$dtb_size" ] || {
	echo 'uImage payload is too small to contain the N437 DTB' >&2
	exit 1
}
tail -c "$dtb_size" "$work/kernel-payload" | cmp -s - "$dtb" || {
	echo 'Exact N437 DTB is not appended to the uImage kernel payload' >&2
	exit 1
}

# No unrelated legacy kernel may survive in the old sector-2048 slot.
dd if="$image" of="$work/legacy-slot" bs=512 skip=2048 count=12284 status=none
[ "$(sha256sum "$work/legacy-slot" | awk '{print $1}')" = \
	"$(dd if=/dev/zero bs=512 count=12284 status=none | sha256sum | awk '{print $1}')" ] || {
	echo 'Legacy sector-2048 kernel slot was not sanitized' >&2
	exit 1
}

# Validate the N437 device-specific waveform copied from the user reference.
"$(CDPATH='' cd -- "$(dirname "$0")" && pwd)/extract-n437-waveform.py" \
	"$image" "$work/epdc.fw" >/dev/null

dd if="$image" of="$work/p1.ext4" bs=512 skip=30720 count=49152 status=none
dd if="$image" of="$work/p2.ext4" bs=512 skip=104448 count=262144 status=none
dd if="$image" of="$work/p3.ext4" bs=512 skip=366592 count=1048576 status=none
dd if="$image" of="$work/p4-superblock" bs=512 skip=1415168 count=8192 status=none
[ "$(e2label "$work/p1.ext4")" = crossink-boot ] || { echo 'p1 label mismatch' >&2; exit 1; }
[ "$(e2label "$work/p2.ext4")" = crossink-recov ] || { echo 'p2 label mismatch' >&2; exit 1; }
[ "$(e2label "$work/p3.ext4")" = crossink-root ] || { echo 'p3 label mismatch' >&2; exit 1; }
[ "$(e2label "$work/p4-superblock")" = crossink-user ] || { echo 'p4 label mismatch' >&2; exit 1; }
debugfs -R 'stat /flags/CROSSINK_RECOVERY_AVAILABLE' "$work/p1.ext4" 2>/dev/null | grep -q 'Inode:' || {
	echo 'CrossInk recovery flag absent from p1' >&2
	exit 1
}

root_dump="$work/root"
mkdir -p "$root_dump"
debugfs -R "rdump / $root_dump" "$work/p3.ext4" >/dev/null 2>&1
[ -x "$root_dump/usr/bin/crossink-kobo" ] || { echo 'CrossInk executable absent from p3' >&2; exit 1; }
file "$root_dump/usr/bin/crossink-kobo" | grep -q 'ELF 32-bit.*ARM' || {
	echo 'CrossInk executable is not an ARM32 ELF' >&2
	exit 1
}
[ "$(sha256sum "$root_dump/lib/firmware/imx/epdc/epdc.fw" | awk '{print $1}')" = \
	'a158cba8276dc5ed5a146f7465285db1741612a5066497d10269f526a597de67' ] || {
	echo 'Validated N437 EPDC firmware absent from p3' >&2
	exit 1
}
grep -q '^root:\*:' "$root_dump/etc/shadow" || { echo 'Root password is not locked' >&2; exit 1; }
grep -q '^ssh-ed25519 ' "$root_dump/root/.ssh/authorized_keys" || {
	echo 'Dedicated public SSH key absent' >&2
	exit 1
}
if find "$root_dump" -type f \( -name 'id_*' -o -name 'pass.txt' \) \
	! -name 'authorized_keys' -print -quit | grep -q .; then
	echo 'Secret-like filename found in rootfs' >&2
	exit 1
fi
if grep -R -I -E -l 'BEGIN (OPENSSH|RSA|EC|DSA) PRIVATE KEY|^[[:space:]]*(ssid|psk|password)=' "$root_dump" \
	2>/dev/null | grep -q .; then
	echo 'Private key or populated Wi-Fi credentials found in rootfs' >&2
	exit 1
fi

echo 'CrossInk N437 image structure: OK'
