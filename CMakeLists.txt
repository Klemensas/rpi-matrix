cmake_minimum_required(VERSION 3.10)
project(rpi-matrix VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_RPI "Build Raspberry Pi targets (libcamera + rgb-led-matrix)" ON)
option(BUILD_DESKTOP "Build Desktop target (OpenCV VideoCapture + software matrix window)" OFF)

if(APPLE)
    set(BUILD_RPI OFF CACHE BOOL "Build Raspberry Pi targets (libcamera + rgb-led-matrix)" FORCE)
    set(BUILD_DESKTOP ON CACHE BOOL "Build Desktop target (OpenCV VideoCapture + software matrix window)" FORCE)
endif()

# Include directories (common)
include_directories(${CMAKE_SOURCE_DIR}/include)

# OpenCV for shared processing
find_package(OpenCV REQUIRED)
message(STATUS "Found OpenCV: ${OpenCV_VERSION}")

set(COMMON_SOURCES
    src/app/app_core.cpp
    src/components/debug_data_collector.cpp
)

if(BUILD_RPI)
    # Find rpi-rgb-led-matrix library
    if(EXISTS "${CMAKE_SOURCE_DIR}/rpi-rgb-led-matrix")
        set(RGB_LED_MATRIX_DIR "${CMAKE_SOURCE_DIR}/rpi-rgb-led-matrix")
        set(RGB_LED_MATRIX_INCLUDE_DIR "${RGB_LED_MATRIX_DIR}/include")
        set(RGB_LED_MATRIX_LIB "${RGB_LED_MATRIX_DIR}/lib/librgbmatrix.a")
    else()
        find_path(RGB_LED_MATRIX_INCLUDE_DIR
            NAMES led-matrix.h
            PATHS /usr/local/include
        )
        find_library(RGB_LED_MATRIX_LIB
            NAMES rgbmatrix
            PATHS /usr/local/lib
        )
    endif()

    if(NOT RGB_LED_MATRIX_INCLUDE_DIR OR NOT RGB_LED_MATRIX_LIB)
        message(FATAL_ERROR "rpi-rgb-led-matrix not found. Please build it first:\n    cd rpi-rgb-led-matrix && make")
    endif()

    message(STATUS "Found rpi-rgb-led-matrix:")
    message(STATUS "  Include: ${RGB_LED_MATRIX_INCLUDE_DIR}")
    message(STATUS "  Library: ${RGB_LED_MATRIX_LIB}")

    # libcamera (Raspberry Pi)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBCAMERA REQUIRED libcamera)
    message(STATUS "Found libcamera:")
    message(STATUS "  Include: ${LIBCAMERA_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${LIBCAMERA_LIBRARIES}")

    set(RPI_SOURCES
        src/camera_to_matrix.cpp
        src/components/camera_capture.cpp
        src/components/matrix_display.cpp
        src/components/debug_overlay.cpp
    )

    add_executable(camera_to_matrix ${COMMON_SOURCES} ${RPI_SOURCES})
    target_include_directories(camera_to_matrix PRIVATE
        ${OpenCV_INCLUDE_DIRS}
        ${RGB_LED_MATRIX_INCLUDE_DIR}
        ${LIBCAMERA_INCLUDE_DIRS}
    )
    target_link_libraries(camera_to_matrix
        ${OpenCV_LIBS}
        ${RGB_LED_MATRIX_LIB}
        ${LIBCAMERA_LIBRARIES}
        pthread
        m
    )

    # Executable: rpicam_to_matrix (stdin -> matrix)
    add_executable(rpicam_to_matrix src/rpicam_to_matrix.cpp)
    target_include_directories(rpicam_to_matrix PRIVATE ${RGB_LED_MATRIX_INCLUDE_DIR})
    target_link_libraries(rpicam_to_matrix ${RGB_LED_MATRIX_LIB} pthread m)
endif()

if(BUILD_DESKTOP)
    set(DESKTOP_SOURCES
        src/desktop_to_matrix.cpp
        src/components/software_matrix_display.cpp
    )

    add_executable(desktop_to_matrix ${COMMON_SOURCES} ${DESKTOP_SOURCES})
    target_include_directories(desktop_to_matrix PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(desktop_to_matrix ${OpenCV_LIBS})
endif()
