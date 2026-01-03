#!/bin/bash
# Build script for rpi-matrix C++ project

set -e

echo "Building rpi-matrix C++ project..."

# Check if rpi-rgb-led-matrix is built
if [ ! -f "rpi-rgb-led-matrix/lib/librgbmatrix.a" ]; then
    echo "Building rpi-rgb-led-matrix library..."
    if [ -d "rpi-rgb-led-matrix" ]; then
        cd rpi-rgb-led-matrix
        make
        cd ..
    else
        echo "Error: rpi-rgb-led-matrix directory not found"
        echo "Please clone it: git clone https://github.com/hzeller/rpi-rgb-led-matrix.git"
        exit 1
    fi
fi

# Check for cmake
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake not found. Install with: sudo apt install cmake"
    exit 1
fi

# Check for libcamera-dev
if ! pkg-config --exists libcamera 2>/dev/null; then
    echo "Error: libcamera-dev not found."
    echo "Install with: sudo apt install libcamera-dev"
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Configure and build
echo "Configuring CMake..."
cmake ..

echo "Building..."
make -j$(nproc)

echo ""
echo "Build complete!"
echo "Executable: build/camera_to_matrix"
echo ""
echo "Run with: sudo ./build/camera_to_matrix"
