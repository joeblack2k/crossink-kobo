cmake_minimum_required(VERSION 3.16)
project(crossink_kobo_platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KOBO_BUILD_DISPLAY "Build the Linux DRM/FBInk display backend" ON)

if(KOBO_BUILD_DISPLAY)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
	set(FBINK_ROOT "" CACHE PATH "FBInk source/install root")
	find_path(FBINK_INCLUDE_DIR fbink.h
		HINTS "${FBINK_ROOT}" "${FBINK_ROOT}/include"
		REQUIRED
	)
	find_library(FBINK_LIBRARY fbink
		HINTS "${FBINK_ROOT}" "${FBINK_ROOT}/Release" "${FBINK_ROOT}/lib"
		REQUIRED
	)
	add_library(crossink_kobo_display STATIC
		display/KoboDrmDisplay.cpp
		display/KoboFbInkDisplay.cpp
		display/KoboPackedMono.cpp
		display/KoboRefreshExecutor.cpp
		display/KoboRefreshProfile.cpp
		display/KoboRefreshQualification.cpp
		display/KoboRefreshScheduler.cpp
	)
	target_include_directories(crossink_kobo_display
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/display>
			$<INSTALL_INTERFACE:include/crossink-kobo>
		PRIVATE
			${FBINK_INCLUDE_DIR}
	)
	target_compile_options(crossink_kobo_display PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_compile_definitions(crossink_kobo_display PRIVATE FBINK_FOR_KOBO)
	target_link_libraries(crossink_kobo_display PUBLIC ${FBINK_LIBRARY} PkgConfig::LIBDRM m pthread rt)

	add_library(crossink_kobo_hal_display STATIC compat/HalDisplay.cpp)
	target_include_directories(crossink_kobo_hal_display
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>
			$<INSTALL_INTERFACE:include/crossink-kobo>
	)
	target_compile_options(crossink_kobo_hal_display PRIVATE -Wall -Wextra -Wpedantic -Werror)
	# HalDisplay records CPU/thermal telemetry for every E-ink submission.  Keep
	# the sysfs HAL explicit so static-library consumers receive its symbols too.
	target_link_libraries(crossink_kobo_hal_display PUBLIC crossink_kobo_display crossink_kobo_sysfs)

	add_executable(crossink-kobo-display-smoke tools/display-smoke.cpp tools/DisplayBenchmark.cpp)
	target_compile_options(crossink-kobo-display-smoke PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_include_directories(crossink-kobo-display-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sysfs" "${CMAKE_CURRENT_SOURCE_DIR}/tools")
	target_link_libraries(crossink-kobo-display-smoke PRIVATE crossink_kobo_display crossink_kobo_sysfs)
endif()

add_library(crossink_kobo_input STATIC
	input/KoboEvdevKey.cpp
	input/KoboEvdevTouch.cpp
	input/KoboTouchGesture.cpp
	input/KoboTouchTransform.cpp
)
target_include_directories(crossink_kobo_input
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/input>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_input PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(KOBO_BUILD_DISPLAY)
	add_executable(crossink-kobo-touch-calibrate tools/touch-calibrate.cpp)
	target_compile_options(crossink-kobo-touch-calibrate PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-touch-calibrate PRIVATE crossink_kobo_display crossink_kobo_input)
endif()

add_executable(crossink-kobo-input-probe tools/input-probe.cpp)
target_compile_options(crossink-kobo-input-probe PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(crossink_kobo_sysfs STATIC sysfs/KoboSysfs.cpp sysfs/KoboCpuFreq.cpp)
target_include_directories(crossink_kobo_sysfs
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sysfs>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_sysfs PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(KOBO_BUILD_DISPLAY)
	target_include_directories(crossink_kobo_display PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sysfs")
	target_link_libraries(crossink_kobo_display PRIVATE crossink_kobo_sysfs)
endif()

add_library(crossink_kobo_suspend STATIC power/KoboSuspendController.cpp)
target_include_directories(crossink_kobo_suspend
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/power>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_suspend PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(crossink_kobo_hal_gpio STATIC compat/HalGPIO.cpp)
target_include_directories(crossink_kobo_hal_gpio
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_hal_gpio PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(crossink_kobo_hal_gpio PUBLIC crossink_kobo_input crossink_kobo_sysfs crossink_kobo_suspend)

add_library(crossink_kobo_hal_power STATIC compat/HalPowerManager.cpp)
target_include_directories(crossink_kobo_hal_power
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_hal_power PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(crossink_kobo_hal_power PUBLIC crossink_kobo_hal_gpio crossink_kobo_sysfs)

add_library(crossink_kobo_hal_clock STATIC compat/HalClock.cpp)
target_include_directories(crossink_kobo_hal_clock
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_hal_clock PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(crossink_kobo_hal_system STATIC compat/HalSystem.cpp)
target_include_directories(crossink_kobo_hal_system
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_hal_system PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(crossink_kobo_hal_tilt STATIC compat/HalTiltSensor.cpp)
target_include_directories(crossink_kobo_hal_tilt
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_hal_tilt PRIVATE -Wall -Wextra -Wpedantic -Werror)

set(KOBO_INSTALL_TARGETS crossink_kobo_hal_clock crossink_kobo_hal_system crossink_kobo_hal_gpio crossink_kobo_suspend
	crossink_kobo_hal_power crossink_kobo_input crossink_kobo_sysfs
	crossink_kobo_hal_tilt crossink-kobo-input-probe)
if(KOBO_BUILD_DISPLAY)
	list(APPEND KOBO_INSTALL_TARGETS crossink_kobo_display crossink_kobo_hal_display crossink-kobo-display-smoke
		crossink-kobo-touch-calibrate)
endif()
install(TARGETS ${KOBO_INSTALL_TARGETS}
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)
install(FILES compat/HalClock.h compat/HalDisplay.h compat/HalGPIO.h compat/HalPowerManager.h compat/HalSystem.h compat/HalTiltSensor.h compat/KoboSystemCompat.h
	display/KoboDisplayTypes.h display/KoboFbInkDisplay.h display/KoboPackedMono.h display/KoboRefreshExecutor.h display/KoboRefreshProfile.h display/KoboRefreshQualification.h display/KoboRefreshScheduler.h input/KoboEvdevAbi.h input/KoboEvdevKey.h input/KoboEvdevTouch.h
	input/KoboTouchGesture.h input/KoboTouchTransform.h sysfs/KoboSysfs.h sysfs/KoboCpuFreq.h power/KoboSuspendController.h
	DESTINATION include/crossink-kobo
)

include(CTest)
if(BUILD_TESTING)
	add_executable(crossink-kobo-capabilities-test tests/capabilities-test.cpp)
	target_include_directories(crossink-kobo-capabilities-test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/compat"
		"${CMAKE_CURRENT_SOURCE_DIR}/input"
		"${CMAKE_CURRENT_SOURCE_DIR}/sysfs"
		"${CMAKE_CURRENT_SOURCE_DIR}/power"
		"${CROSSINK_ROOT}/src"
	)
	target_compile_options(crossink-kobo-capabilities-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-capabilities-test PRIVATE crossink_kobo_hal_gpio)
	add_test(NAME crossink-kobo-capabilities COMMAND crossink-kobo-capabilities-test)

	add_executable(crossink-kobo-system-memory-test tests/system-memory-test.cpp)
	target_compile_options(crossink-kobo-system-memory-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-system-memory-test PRIVATE crossink_kobo_hal_system)
	add_test(NAME crossink-kobo-system-memory COMMAND crossink-kobo-system-memory-test)

	add_executable(crossink-kobo-touch-transform-test tests/touch-transform-test.cpp)
	target_compile_options(crossink-kobo-touch-transform-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-touch-transform-test PRIVATE crossink_kobo_input)
	add_test(NAME crossink-kobo-touch-transform COMMAND crossink-kobo-touch-transform-test)

	add_executable(crossink-kobo-touch-gesture-test tests/touch-gesture-test.cpp)
	target_compile_options(crossink-kobo-touch-gesture-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-touch-gesture-test PRIVATE crossink_kobo_input)
	add_test(NAME crossink-kobo-touch-gesture COMMAND crossink-kobo-touch-gesture-test)

	add_executable(crossink-kobo-touch-ui-registry-test tests/touch-ui-registry-test.cpp
		"${CMAKE_CURRENT_SOURCE_DIR}/../../src/components/TouchUiRegistry.cpp")
	target_include_directories(crossink-kobo-touch-ui-registry-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
	target_compile_options(crossink-kobo-touch-ui-registry-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	add_test(NAME crossink-kobo-touch-ui-registry COMMAND crossink-kobo-touch-ui-registry-test)

	add_executable(crossink-kobo-evdev-key-test tests/evdev-key-test.cpp)
	target_compile_options(crossink-kobo-evdev-key-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-evdev-key-test PRIVATE crossink_kobo_input)
	add_test(NAME crossink-kobo-evdev-key COMMAND crossink-kobo-evdev-key-test)

	add_executable(crossink-kobo-evdev-touch-test tests/evdev-touch-test.cpp)
	target_compile_options(crossink-kobo-evdev-touch-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-evdev-touch-test PRIVATE crossink_kobo_input)
	add_test(NAME crossink-kobo-evdev-touch COMMAND crossink-kobo-evdev-touch-test)

	add_executable(crossink-kobo-sysfs-test tests/sysfs-test.cpp)
	target_compile_options(crossink-kobo-sysfs-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-sysfs-test PRIVATE crossink_kobo_sysfs)
	add_test(NAME crossink-kobo-sysfs COMMAND crossink-kobo-sysfs-test)

	add_executable(crossink-kobo-packed-mono-test tests/packed-mono-test.cpp display/KoboPackedMono.cpp)
	target_include_directories(crossink-kobo-packed-mono-test PRIVATE display)
	target_compile_options(crossink-kobo-packed-mono-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	add_test(NAME crossink-kobo-packed-mono COMMAND crossink-kobo-packed-mono-test)

	add_executable(crossink-kobo-refresh-profile-test tests/refresh-profile-test.cpp display/KoboRefreshProfile.cpp)
	target_include_directories(crossink-kobo-refresh-profile-test PRIVATE display)
	target_compile_options(crossink-kobo-refresh-profile-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	add_test(NAME crossink-kobo-refresh-profile COMMAND crossink-kobo-refresh-profile-test)

	add_executable(crossink-kobo-cpufreq-test tests/cpufreq-test.cpp)
	target_compile_options(crossink-kobo-cpufreq-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-cpufreq-test PRIVATE crossink_kobo_sysfs)
	add_test(NAME crossink-kobo-cpufreq COMMAND crossink-kobo-cpufreq-test)

	add_executable(crossink-kobo-refresh-scheduler-test tests/refresh-scheduler-test.cpp
		display/KoboRefreshProfile.cpp display/KoboRefreshScheduler.cpp)
	target_include_directories(crossink-kobo-refresh-scheduler-test PRIVATE display)
	target_compile_options(crossink-kobo-refresh-scheduler-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	add_test(NAME crossink-kobo-refresh-scheduler COMMAND crossink-kobo-refresh-scheduler-test)

	add_executable(crossink-kobo-display-benchmark-test tests/display-benchmark-test.cpp tools/DisplayBenchmark.cpp)
	target_include_directories(crossink-kobo-display-benchmark-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tools")
	target_compile_options(crossink-kobo-display-benchmark-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	add_test(NAME crossink-kobo-display-benchmark COMMAND crossink-kobo-display-benchmark-test)

	add_executable(crossink-kobo-render-scheduler-test tests/render-scheduler-test.cpp compat/KoboRenderScheduler.cpp)
	target_include_directories(crossink-kobo-render-scheduler-test PRIVATE compat)
	target_compile_options(crossink-kobo-render-scheduler-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-render-scheduler-test PRIVATE pthread)
	add_test(NAME crossink-kobo-render-scheduler COMMAND crossink-kobo-render-scheduler-test)

	add_executable(crossink-kobo-opds-book-storage-test tests/opds-book-storage-test.cpp
		"${CROSSINK_ROOT}/src/network/OpdsBookStorage.cpp"
		"${CROSSINK_ROOT}/src/util/StringUtils.cpp"
		"${CROSSINK_ROOT}/lib/Utf8/Utf8.cpp")
	target_include_directories(crossink-kobo-opds-book-storage-test PRIVATE
		"${CROSSINK_ROOT}/src" "${CROSSINK_ROOT}/lib/Utf8")
	target_compile_options(crossink-kobo-opds-book-storage-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	add_test(NAME crossink-kobo-opds-book-storage COMMAND crossink-kobo-opds-book-storage-test)
endif()
