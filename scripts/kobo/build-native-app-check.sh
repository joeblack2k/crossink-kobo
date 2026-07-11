#!/bin/sh
set -u

workspace="${CROSSINK_NATIVE_WORKSPACE:-/opt/crossink-kobo-build}"
source_dir="${CROSSINK_NATIVE_SOURCE:-$workspace/src}"
build_dir="${CROSSINK_NATIVE_BUILD_DIR:-$workspace/native-app-build}"
log_file="${CROSSINK_NATIVE_LOG:-$workspace/native-app.log}"
exit_file="${CROSSINK_NATIVE_EXIT:-$workspace/native-app.exit}"

rm -f "$exit_file"
cmake -S "$source_dir/platform/kobo/app" -B "$build_dir" \
	-DCROSSINK_ROOT="$source_dir" \
	-DCROSSPOINT_SIMULATOR_ROOT="$workspace/vendor/crosspoint-simulator" \
	-DARDUINOJSON_ROOT="$workspace/vendor/ArduinoJson" \
	-DQRCODE_ROOT="$workspace/vendor/QRCode" \
	-DPNGDEC_ROOT="$workspace/vendor/PNGdec" \
	-DJPEGDEC_ROOT="$workspace/vendor/JPEGDEC" \
	-DFBINK_ROOT="$workspace/vendor/FBInk" \
	-DBUILD_TESTING=OFF >"$log_file" 2>&1 && \
	cmake --build "$build_dir" --target crossink-kobo \
		"-j${JOBS:-2}" >>"$log_file" 2>&1
code=$?
printf '%s\n' "$code" >"$exit_file"
exit "$code"
