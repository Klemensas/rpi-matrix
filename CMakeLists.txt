cmake_minimum_required(VERSION 3.10)
project(rpi-matrix VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find rpi-rgb-led-matrix library
# Check if it's in the parent directory or installed system-wide
if(EXISTS "${CMAKE_SOURCE_DIR}/rpi-rgb-led-matrix")
    set(RGB_LED_MATRIX_DIR "${CMAKE_SOURCE_DIR}/rpi-rgb-led-matrix")
    set(RGB_LED_MATRIX_INCLUDE_DIR "${RGB_LED_MATRIX_DIR}/include")
    set(RGB_LED_MATRIX_LIB "${RGB_LED_MATRIX_DIR}/lib/librgbmatrix.a")
else()
    # Try system installation
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
    message(FATAL_ERROR "rpi-rgb-led-matrix not found. Please build it first:
    cd rpi-rgb-led-matrix && make")
endif()

message(STATUS "Found rpi-rgb-led-matrix:")
message(STATUS "  Include: ${RGB_LED_MATRIX_INCLUDE_DIR}")
message(STATUS "  Library: ${RGB_LED_MATRIX_LIB}")

# Include directories
include_directories(${RGB_LED_MATRIX_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(CAMERA_SOURCES 
    src/camera_to_matrix.cpp
    src/components/camera_capture.cpp
    src/components/matrix_display.cpp
)
set(RPICAM_SOURCES src/rpicam_to_matrix.cpp)

# Executable: camera_to_matrix (direct libcamera)
add_executable(camera_to_matrix ${CAMERA_SOURCES})

# Link libraries for camera_to_matrix
target_link_libraries(camera_to_matrix
    ${RGB_LED_MATRIX_LIB}
    pthread
    m
)

# Raspberry Pi camera libraries (libcamera) - REQUIRED
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)

if(NOT LIBCAMERA_FOUND)
    message(FATAL_ERROR "libcamera not found. Install with: sudo apt install libcamera-dev")
endif()

target_include_directories(camera_to_matrix PRIVATE ${LIBCAMERA_INCLUDE_DIRS})
target_link_libraries(camera_to_matrix ${LIBCAMERA_LIBRARIES})
message(STATUS "Found libcamera:")
message(STATUS "  Include: ${LIBCAMERA_INCLUDE_DIRS}")
message(STATUS "  Libraries: ${LIBCAMERA_LIBRARIES}")

# libcamera-apps is optional but recommended
pkg_check_modules(LIBCAMERA_APPS libcamera-apps)
if(LIBCAMERA_APPS_FOUND)
    target_link_libraries(camera_to_matrix ${LIBCAMERA_APPS_LIBRARIES})
    message(STATUS "Found libcamera-apps")
endif()

# OpenCV for image processing and person detection
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "Found OpenCV: ${OpenCV_VERSION}")
    target_include_directories(camera_to_matrix PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(camera_to_matrix ${OpenCV_LIBS})
else()
    message(FATAL_ERROR "OpenCV not found. Install with: sudo apt install libopencv-dev")
endif()

# Executable: rpicam_to_matrix (reads from stdin)
add_executable(rpicam_to_matrix ${RPICAM_SOURCES})

# Link libraries for rpicam_to_matrix
target_link_libraries(rpicam_to_matrix
    ${RGB_LED_MATRIX_LIB}
    pthread
    m
)
