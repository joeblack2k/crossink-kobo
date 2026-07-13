#!/bin/sh
set -eu

repo_dir="$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)"
buildroot_dir="$repo_dir/vendor/buildroot"
external_dir="$repo_dir/buildroot-external"
output_dir="${CROSSINK_KOBO_OUTPUT_DIR:-$repo_dir/output-kobo}"
key_file="${CROSSINK_SSH_PUBLIC_KEY_FILE:-$repo_dir/local-secrets/crossink_n437_ed25519.pub}"
reference_image="${CROSSINK_N437_REFERENCE_IMAGE:-}"
waveform_file="$output_dir/n437-device-data/epdc.fw"

expected_buildroot=92abb11ee4f574981eb194d283b97afad5dec004
expected_kernel=ccddaba42e0abafbce84cc2243cbe0c98d400944
expected_fbink=83110d3d278cf9cd44cc1d16237e284a89f72633
expected_simulator=efe37c6779e1f977cb361240f56ba4028d1c13f6
expected_arduinojson=733bc4ee82630c88c0a619a883cd3a206efae977
expected_qrcode=5ba5d5bf2790f4a885b0db823cf218e4fa718061
expected_pngdec=9a9c585fd39d148c5517597b02ce490e0fdb1bb4
expected_jpegdec=86282979224c8a32fd51e091ed5a35b0c699a52b

[ "$(git -C "$buildroot_dir" rev-parse HEAD)" = "$expected_buildroot" ] || {
	echo "Buildroot checkout does not match sources.lock" >&2
	exit 1
}
grep -q "$expected_kernel" "$external_dir/configs/crossink_kobo_glo_hd_defconfig" || {
	echo "Kernel pin does not match sources.lock" >&2
	exit 1
}
[ "$(git -C "$repo_dir/vendor/FBInk" rev-parse HEAD)" = "$expected_fbink" ] || {
	echo "FBInk checkout does not match sources.lock" >&2
	exit 1
}
if git -C "$repo_dir/vendor/FBInk" submodule status --recursive | grep -Eq '^[-+U]'; then
	echo "FBInk submodules are missing, modified, or at the wrong commit" >&2
	exit 1
fi
for dependency in \
	"crosspoint-simulator:$expected_simulator" \
	"ArduinoJson:$expected_arduinojson" \
	"QRCode:$expected_qrcode" \
	"PNGdec:$expected_pngdec" \
	"JPEGDEC:$expected_jpegdec"
do
	directory=${dependency%%:*}
	expected=${dependency#*:}
	[ "$(git -C "$repo_dir/vendor/$directory" rev-parse HEAD)" = "$expected" ] || {
		echo "$directory checkout does not match sources.lock" >&2
		exit 1
	}
done
"$repo_dir/scripts/kobo/apply-vendor-patches.sh"
python3 "$repo_dir/scripts/build_web.py"
[ -f "$key_file" ] || {
	echo "Missing dedicated SSH public key: $key_file" >&2
	exit 1
}
[ -n "$reference_image" ] && [ -f "$reference_image" ] || {
	echo "CROSSINK_N437_REFERENCE_IMAGE must name the full user-owned N437 SD image" >&2
	exit 1
}
python3 "$repo_dir/scripts/kobo/extract-n437-waveform.py" \
	"$reference_image" "$waveform_file"

gnu_path=""
for path in \
	/opt/homebrew/opt/coreutils/libexec/gnubin \
	/opt/homebrew/opt/gnu-sed/libexec/gnubin \
	/opt/homebrew/opt/gnu-tar/libexec/gnubin \
	/opt/homebrew/opt/findutils/libexec/gnubin \
	/opt/homebrew/opt/gpatch/libexec/gnubin
do
	[ -d "$path" ] && gnu_path="$gnu_path:$path"
done
if [ -n "$gnu_path" ]; then
	PATH="${gnu_path#:}:$PATH"
fi
export PATH CROSSINK_SSH_PUBLIC_KEY_FILE="$key_file" \
	CROSSINK_EPDC_FIRMWARE_FILE="$waveform_file"

if [ "$(uname -s)" = Darwin ]; then
	hostcc="${HOSTCC:-$(command -v gcc-16 || command -v gcc)}"
	hostcxx="${HOSTCXX:-$(command -v g++-16 || command -v g++)}"
	host_cflags="${HOST_CFLAGS:--O2}"
	host_ldflags="${HOST_LDFLAGS:-}"
	sdk="$(xcrun --show-sdk-path)"
	host_cflags="$host_cflags -isysroot $sdk"
	host_ldflags="$host_ldflags -isysroot $sdk"
	buildroot_make() {
		make -C "$buildroot_dir" BR2_EXTERNAL="$external_dir" O="$output_dir" \
			HOSTCC="$hostcc" HOSTCXX="$hostcxx" \
			HOST_CFLAGS="$host_cflags" HOST_LDFLAGS="$host_ldflags" "$@"
	}
else
	# Buildroot supplies Linux host RPATH/link flags itself. Passing an empty
	# HOST_LDFLAGS strips -L$(HOST_DIR)/lib and makes host tools fail to link.
	buildroot_make() {
		make -C "$buildroot_dir" BR2_EXTERNAL="$external_dir" O="$output_dir" "$@"
	}
fi

buildroot_make crossink_kobo_glo_hd_defconfig
if [ "${CROSSINK_LINUX_RECONFIGURE:-0}" = 1 ]; then
	buildroot_make linux-reconfigure
fi
# Buildroot local packages are not content-addressed.  Do not invoke a
# *-rebuild target here: when the output directory is fresh, that target
# bypasses dependency resolution and CMake can run before libdrm exists in the
# staging sysroot.  Remove only the changed local package directories, then
# let the normal top-level build resolve every dependency in order.
if [ "${CROSSINK_FORCE_APP_REBUILD:-0}" = 1 ]; then
	buildroot_make crossink-kobo-app-dirclean
fi
if [ "${CROSSINK_FORCE_PLATFORM_REBUILD:-0}" = 1 ]; then
	buildroot_make crossink-kobo-platform-dirclean
fi
buildroot_make "-j${JOBS:-6}"
