#!/bin/sh
set -u

# Default to this checkout.  The LXC can still set CROSSINK_NATIVE_WORKSPACE
# explicitly, but a developer/CI invocation must never silently target an
# unrelated /opt tree.
repo_dir="$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)"
workspace="${CROSSINK_NATIVE_WORKSPACE:-$repo_dir}"
# The LXC checkout is the source root.  A separate source directory can still
# be supplied explicitly for CI, but `$workspace/src` contains only C++ source
# files and not the scripts/platform CMake root this checker needs.
source_dir="${CROSSINK_NATIVE_SOURCE:-$workspace}"
build_dir="${CROSSINK_NATIVE_BUILD_DIR:-$workspace/native-app-build}"
log_file="${CROSSINK_NATIVE_LOG:-$workspace/native-app.log}"
exit_file="${CROSSINK_NATIVE_EXIT:-$workspace/native-app.exit}"

rm -f "$exit_file"
python3 "$source_dir/scripts/build_web.py" >"$log_file" 2>&1
"$source_dir/scripts/kobo/apply-vendor-patches.sh" || exit $?
cmake -S "$source_dir/platform/kobo/app" -B "$build_dir" \
	-DCROSSINK_ROOT="$source_dir" \
	-DCROSSPOINT_SIMULATOR_ROOT="$workspace/vendor/crosspoint-simulator" \
	-DARDUINOJSON_ROOT="$workspace/vendor/ArduinoJson" \
	-DQRCODE_ROOT="$workspace/vendor/QRCode" \
	-DPNGDEC_ROOT="$workspace/vendor/PNGdec" \
	-DJPEGDEC_ROOT="$workspace/vendor/JPEGDEC" \
	-DFBINK_ROOT="$workspace/vendor/FBInk" \
	-DBUILD_TESTING=OFF >"$log_file" 2>&1 && \
	python3 "$source_dir/scripts/kobo/audit-kobo-dependencies.py" \
		--compile-commands "$build_dir/compile_commands.json" \
		--source-root "$source_dir" >>"$log_file" 2>&1 && \
	cmake --build "$build_dir" --target crossink-kobo \
		"-j${JOBS:-2}" >>"$log_file" 2>&1
code=$?
printf '%s\n' "$code" >"$exit_file"
exit "$code"
