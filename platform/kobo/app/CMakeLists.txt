cmake_minimum_required(VERSION 3.16)
project(crossink_kobo_app LANGUAGES C CXX)

# The target-aware dependency audit consumes exactly this database.  It is
# deliberately enabled for Buildroot as well as developer CMake builds, so CI
# cannot accidentally audit a host/simulator source list instead of Kobo's.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CROSSINK_ROOT "" CACHE PATH "CrossInk source root")
set(CROSSPOINT_SIMULATOR_ROOT "" CACHE PATH "Pinned crosspoint-simulator source root")
set(ARDUINOJSON_ROOT "" CACHE PATH "ArduinoJson package root")
set(QRCODE_ROOT "" CACHE PATH "QRCode package root")
set(PNGDEC_ROOT "" CACHE PATH "PNGdec package root")
set(JPEGDEC_ROOT "" CACHE PATH "JPEGDEC package root")

foreach(required CROSSINK_ROOT CROSSPOINT_SIMULATOR_ROOT ARDUINOJSON_ROOT QRCODE_ROOT PNGDEC_ROOT JPEGDEC_ROOT)
	if(NOT IS_DIRECTORY "${${required}}")
		message(FATAL_ERROR "${required} must name a pinned source directory")
	endif()
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
	COMMAND "${Python3_EXECUTABLE}" "${CROSSINK_ROOT}/scripts/gen_i18n.py"
		"${CROSSINK_ROOT}/lib/I18n/translations" "${CROSSINK_ROOT}/lib/I18n"
	WORKING_DIRECTORY "${CROSSINK_ROOT}"
	RESULT_VARIABLE i18n_result
	OUTPUT_QUIET
)
if(NOT i18n_result EQUAL 0)
	message(FATAL_ERROR "CrossInk i18n source generation failed (${i18n_result})")
endif()

file(GLOB_RECURSE crossink_sources CONFIGURE_DEPENDS
	"${CROSSINK_ROOT}/src/*.cpp"
	"${CROSSINK_ROOT}/lib/*.c"
	"${CROSSINK_ROOT}/lib/*.cpp"
)
# macOS can leave AppleDouble metadata beside a source file when an archive is
# unpacked on a Linux build worker.  Those files start with `._` and CMake's
# glob would otherwise compile their binary metadata as C++.
list(FILTER crossink_sources EXCLUDE REGEX "/\\._")
list(FILTER crossink_sources EXCLUDE REGEX "/lib/hal/")
list(FILTER crossink_sources EXCLUDE REGEX "/lib/HalClockSim/")
list(FILTER crossink_sources EXCLUDE REGEX "/test/")
list(FILTER crossink_sources EXCLUDE REGEX "/tools/")
list(FILTER crossink_sources EXCLUDE REGEX "/src/network/(CrossPointWebServer|FirmwareFlasher|OtaBootSwitch|OtaUpdater|WebDAVHandler)\\.cpp$")
# Kobo never writes ESP OTA partitions.  The System menu deliberately hides
# these activities, so compiling their simulator stubs into the device would
# only leave unreachable ESP restart/partition code in the production binary.
list(FILTER crossink_sources EXCLUDE REGEX "/src/activities/settings/(OtaUpdateActivity|SdFirmwareUpdateActivity)\\.cpp$")
# Kobo uses the persistent native HTTP transfer service. The ESP USB CDC
# command protocol has no hardware transport on N437 and must not pull the
# simulator HWCDC shim into the production binary.
list(FILTER crossink_sources EXCLUDE REGEX "UsbSerialFileTransfer")
list(FILTER crossink_sources EXCLUDE REGEX "/src/platform/skip_efuse_blk_check\\.c$")
list(FILTER crossink_sources EXCLUDE REGEX "/src/simulator/")

file(GLOB simulator_sources CONFIGURE_DEPENDS "${CROSSPOINT_SIMULATOR_ROOT}/src/*.cpp")
list(FILTER simulator_sources EXCLUDE REGEX "/\\._")
list(FILTER simulator_sources EXCLUDE REGEX "/(HalClock|HalDisplay|HalGPIO|HalPowerManager|HalSystem|HalTiltSensor|SimulatorSmokeTest|firmware_link_stubs|simulator_firmware|qrcode|simulator_main)\\.cpp$")

file(GLOB_RECURSE dependency_sources CONFIGURE_DEPENDS
	"${QRCODE_ROOT}/src/*.c"
	"${QRCODE_ROOT}/src/*.cpp"
	"${PNGDEC_ROOT}/src/*.c"
	"${PNGDEC_ROOT}/src/*.cpp"
	"${JPEGDEC_ROOT}/src/*.c"
	"${JPEGDEC_ROOT}/src/*.cpp"
)
list(FILTER dependency_sources EXCLUDE REGEX "/\\._")

function(collect_header_directories output)
	set(result "")
	foreach(root IN LISTS ARGN)
		file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${root}/*.h" "${root}/*.hpp")
		foreach(header IN LISTS headers)
			get_filename_component(directory "${header}" DIRECTORY)
			list(APPEND result "${directory}")
		endforeach()
	endforeach()
	list(REMOVE_DUPLICATES result)
	set(${output} "${result}" PARENT_SCOPE)
endfunction()

collect_header_directories(crossink_include_dirs
	"${CROSSINK_ROOT}/include"
	"${CROSSINK_ROOT}/src"
	"${CROSSINK_ROOT}/lib"
)

add_subdirectory("${CROSSINK_ROOT}/platform/kobo" "${CMAKE_CURRENT_BINARY_DIR}/kobo-platform" EXCLUDE_FROM_ALL)

add_executable(crossink-kobo
	${crossink_sources}
	${simulator_sources}
	${dependency_sources}
	"${CMAKE_CURRENT_SOURCE_DIR}/kobo-main.cpp"
	"${CROSSINK_ROOT}/platform/kobo/compat/WiFi.cpp"
	"${CROSSINK_ROOT}/platform/kobo/compat/KoboRenderScheduler.cpp"
	"${CROSSINK_ROOT}/platform/kobo/compat/KoboWifiAutoConnect.cpp"
	"${CROSSINK_ROOT}/platform/kobo/compat/KoboWebTransferService.cpp"
)
target_include_directories(crossink-kobo PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CROSSINK_ROOT}/platform/kobo/compat"
	"${CROSSINK_ROOT}/platform/kobo/display"
	"${CROSSINK_ROOT}/platform/kobo/input"
	"${CROSSINK_ROOT}/platform/kobo/sysfs"
	"${CROSSINK_ROOT}/include"
	"${CROSSINK_ROOT}/src"
	"${ARDUINOJSON_ROOT}/src"
	"${QRCODE_ROOT}/src"
	"${PNGDEC_ROOT}/src"
	"${JPEGDEC_ROOT}/src"
	"${CROSSPOINT_SIMULATOR_ROOT}/src"
	${crossink_include_dirs}
)
target_compile_definitions(crossink-kobo PRIVATE
	# Kobo is a POSIX host for Arduino compatibility only.  It must never select
	# desktop-simulator product behaviour (fake heap/restart, smoke harness,
	# ESP-NOW substitutes) merely because it reuses compatibility headers.
	CROSSPOINT_POSIX
	KOBO_LINUX
	CROSSINK_VERSION="1.4.0-kobo-beta1"
	CROSSPOINT_FIRMWARE_VARIANT="kobo-n437"
	CROSSPOINT_NATIVE_BIND_ANY=1
	CROSSPOINT_NATIVE_WEB_PORT=80
	ENABLE_SERIAL_LOG
	LOG_LEVEL=2
	EINK_DISPLAY_SINGLE_BUFFER_MODE=1
	MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1
	XML_GE=0
	XML_CONTEXT_BYTES=1024
	USE_UTF8_LONG_NAMES=1
	PNG_MAX_BUFFERED_PIXELS=16416
	DISABLE_FS_H_WARNING=1
	DESTRUCTOR_CLOSES_FILE=1
	CROSSPOINT_SIM_USE_NATIVE_DECODERS
	memcpy_P=memcpy
)
target_compile_options(crossink-kobo PRIVATE -Wall -Wextra -Wno-narrowing -fno-exceptions)
target_link_libraries(crossink-kobo PRIVATE
	crossink_kobo_hal_display
	crossink_kobo_hal_clock
	crossink_kobo_hal_system
	crossink_kobo_hal_gpio
	crossink_kobo_hal_power
	crossink_kobo_hal_tilt
	pthread
	dl
	OpenSSL::SSL
	OpenSSL::Crypto
)

install(TARGETS crossink-kobo RUNTIME DESTINATION bin)
