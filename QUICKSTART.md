# Quick Start Guide

## Prerequisites

1. **Install system dependencies**:
   ```bash
   sudo apt update
   sudo apt install -y cmake build-essential libcamera-dev libcamera-apps pkg-config
   ```

2. **Enable camera**:
   ```bash
   sudo raspi-config
   # Interface Options → Camera → Enable
   ```

3. **rpi-rgb-led-matrix** (already in this directory):
   - Library is already built ✓
   - If you need to rebuild: `cd rpi-rgb-led-matrix && make`

## Building

```bash
# Simple build
make

# Or use the build script directly
./build.sh
```

## Running

```bash
# Basic usage (default: 640x480, single 64x64 matrix)
sudo ./build/camera_to_matrix

# With options
sudo ./build/camera_to_matrix --width 640 --height 480 --rows 32 --cols 32

# Help
./build/camera_to_matrix --help
```

**Note**: Requires `sudo` for GPIO access.

## Common Configurations

### Single 64x64 Matrix (Default)
```bash
sudo ./build/camera_to_matrix
```

### Chain of 64x64 Matrices
```bash
sudo ./build/camera_to_matrix --rows 64 --cols 64 --chain 2
```

### 32x32 Matrix
```bash
sudo ./build/camera_to_matrix --rows 32 --cols 32
```

### Adafruit HAT
```bash
sudo ./build/camera_to_matrix --hardware-mapping adafruit-hat
```

## Troubleshooting

### Camera not found
```bash
# Test camera
libcamera-hello --list-cameras
libcamera-hello
```

### Build errors
- Ensure rpi-rgb-led-matrix is built: `cd rpi-rgb-led-matrix && make`
- Check cmake is installed: `cmake --version`
- Check libcamera-dev is installed: `pkg-config --exists libcamera && echo "OK"`

### Matrix not displaying
- Must run with `sudo`
- Check GPIO connections
- Verify matrix configuration matches hardware
