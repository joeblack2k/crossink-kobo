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
	target_link_libraries(crossink_kobo_hal_display PUBLIC crossink_kobo_display)

	add_executable(crossink-kobo-display-smoke tools/display-smoke.cpp)
	target_compile_options(crossink-kobo-display-smoke PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-display-smoke PRIVATE crossink_kobo_display)
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

add_library(crossink_kobo_sysfs STATIC sysfs/KoboSysfs.cpp)
target_include_directories(crossink_kobo_sysfs
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sysfs>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_sysfs PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(crossink_kobo_hal_gpio STATIC compat/HalGPIO.cpp)
target_include_directories(crossink_kobo_hal_gpio
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_hal_gpio PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(crossink_kobo_hal_gpio PUBLIC crossink_kobo_input crossink_kobo_sysfs)

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

add_library(crossink_kobo_hal_tilt STATIC compat/HalTiltSensor.cpp)
target_include_directories(crossink_kobo_hal_tilt
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>
		$<INSTALL_INTERFACE:include/crossink-kobo>
)
target_compile_options(crossink_kobo_hal_tilt PRIVATE -Wall -Wextra -Wpedantic -Werror)

set(KOBO_INSTALL_TARGETS crossink_kobo_hal_clock crossink_kobo_hal_gpio
	crossink_kobo_hal_power crossink_kobo_input crossink_kobo_sysfs
	crossink_kobo_hal_tilt)
if(KOBO_BUILD_DISPLAY)
	list(APPEND KOBO_INSTALL_TARGETS crossink_kobo_display crossink_kobo_hal_display crossink-kobo-display-smoke)
endif()
install(TARGETS ${KOBO_INSTALL_TARGETS}
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)
install(FILES compat/HalClock.h compat/HalDisplay.h compat/HalGPIO.h compat/HalPowerManager.h compat/HalTiltSensor.h
	display/KoboFbInkDisplay.h display/KoboPackedMono.h input/KoboEvdevKey.h input/KoboEvdevTouch.h
	input/KoboTouchGesture.h input/KoboTouchTransform.h sysfs/KoboSysfs.h
	DESTINATION include/crossink-kobo
)

include(CTest)
if(BUILD_TESTING)
	add_executable(crossink-kobo-touch-transform-test tests/touch-transform-test.cpp)
	target_compile_options(crossink-kobo-touch-transform-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-touch-transform-test PRIVATE crossink_kobo_input)
	add_test(NAME crossink-kobo-touch-transform COMMAND crossink-kobo-touch-transform-test)

	add_executable(crossink-kobo-touch-gesture-test tests/touch-gesture-test.cpp)
	target_compile_options(crossink-kobo-touch-gesture-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-touch-gesture-test PRIVATE crossink_kobo_input)
	add_test(NAME crossink-kobo-touch-gesture COMMAND crossink-kobo-touch-gesture-test)

	add_executable(crossink-kobo-evdev-key-test tests/evdev-key-test.cpp)
	target_compile_options(crossink-kobo-evdev-key-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-evdev-key-test PRIVATE crossink_kobo_input)
	add_test(NAME crossink-kobo-evdev-key COMMAND crossink-kobo-evdev-key-test)

	add_executable(crossink-kobo-sysfs-test tests/sysfs-test.cpp)
	target_compile_options(crossink-kobo-sysfs-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	target_link_libraries(crossink-kobo-sysfs-test PRIVATE crossink_kobo_sysfs)
	add_test(NAME crossink-kobo-sysfs COMMAND crossink-kobo-sysfs-test)

	add_executable(crossink-kobo-packed-mono-test tests/packed-mono-test.cpp display/KoboPackedMono.cpp)
	target_include_directories(crossink-kobo-packed-mono-test PRIVATE display)
	target_compile_options(crossink-kobo-packed-mono-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
	add_test(NAME crossink-kobo-packed-mono COMMAND crossink-kobo-packed-mono-test)
endif()
