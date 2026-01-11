# rpi-matrix

A C++ project for controlling RGB LED matrices with camera input on Raspberry Pi. This project captures video from the Raspberry Pi Camera Module 3 (Noir) and displays it on RGB LED matrices using the [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix) library.

## Features

- **C++ Implementation**: High-performance native C++ code
- **Camera Integration**: Supports Raspberry Pi Camera Module 3 (Noir version) via libcamera
- **LED Matrix Control**: Uses rpi-rgb-led-matrix library for hardware control
- **Flexible Configuration**: Configurable matrix chains, parallel displays, and resolutions
- **Command-line Interface**: Easy-to-use command-line options

## Project Structure

```
rpi-matrix/
├── src/                    # C++ source files
│   ├── camera_to_matrix.cpp    # Direct libcamera capture
│   └── rpicam_to_matrix.cpp    # Reads rpicam-vid output from stdin
├── include/                # C++ header files (if needed)
├── build/                  # Build output directory
├── rpi-rgb-led-matrix/     # RGB LED matrix library (submodule or clone)
├── CMakeLists.txt          # CMake build configuration
├── Makefile                # Convenience build script
├── build.sh                # Build script
└── README.md               # This file
```

## Prerequisites

### Hardware
- Raspberry Pi (recommended: Pi 4 or newer)
- Raspberry Pi Camera Module 3 (Noir version)
- RGB LED Matrix panel(s) compatible with rpi-rgb-led-matrix
- Appropriate wiring or HAT (e.g., Adafruit RGB Matrix HAT)

### Software Dependencies

1. **rpi-rgb-led-matrix library**:
   ```bash
   git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
   cd rpi-rgb-led-matrix
   make
   # The library will be used from this directory
   ```

2. **System packages**:
   ```bash
   sudo apt update
   sudo apt install -y \
     cmake \
     build-essential \
     libcamera-dev \
     libcamera-apps \
     pkg-config \
     ffmpeg \
     libavdevice-dev \
     libavformat-dev \
     libavcodec-dev \
     libavutil-dev
   ```
   
   **Note**: FFmpeg libraries are required for `rpicam_to_matrix` if you want to use the `video-viewer` utility for comparison.

3. **Enable Camera**:
   ```bash
   sudo raspi-config
   # Navigate to: Interface Options → Camera → Enable
   # Reboot if prompted
   ```

## Building

### Quick Build

```bash
# Using the build script
./build.sh

# Or using Make
make
```

The build script will:
1. Build rpi-rgb-led-matrix if not already built
2. Configure CMake
3. Compile the project

### Manual Build

```bash
# Build rpi-rgb-led-matrix first
cd rpi-rgb-led-matrix
make
cd ..

# Create build directory
mkdir -p build
cd build

# Configure
cmake ..

# Build
make -j$(nproc)
```

## Usage

The project provides two executables:

1. **`camera_to_matrix`**: Direct libcamera capture (recommended for best performance)
2. **`rpicam_to_matrix`**: Reads rpicam-vid output from stdin (useful for comparison/testing)

## Desktop/macOS (software matrix preview)

The effect pipeline is shared in `AppCore` (OpenCV-only), so you can run a **software matrix preview** on a desktop (including macOS) for behavior/performance comparisons.

### Build on macOS

Install dependencies:

```bash
brew install opencv cmake
```

Build:

```bash
cmake -S . -B build -DBUILD_DESKTOP=ON -DBUILD_RPI=OFF
cmake --build build -j
```

Run (webcam):

```bash
./build/desktop_to_matrix --device 0
```

Run (video file):

```bash
./build/desktop_to_matrix --video /path/to/video.mp4
```

Keys: `1-5` switch modes, `q`/`ESC` quit.

### camera_to_matrix (Direct Capture)

#### Basic Usage

```bash
# Run with default settings (640x480, single 64x64 matrix)
sudo ./build/camera_to_matrix
```

**Note**: Requires `sudo` for GPIO access.

#### Command-line Options

```bash
sudo ./build/camera_to_matrix [options]

Options:
  --width WIDTH          Camera capture width (default: 640)
  --height HEIGHT        Camera capture height (default: 480)
  --rows ROWS            Matrix rows per panel (default: 64)
  --cols COLS            Matrix columns per panel (default: 64)
  --chain CHAIN          Number of chained matrices (default: 1)
  --parallel PARALLEL    Number of parallel chains (default: 1)
  --hardware-mapping MAP Hardware mapping: regular, adafruit-hat, adafruit-hat-pwm (default: regular)
  --help                 Show help message
```

#### Examples

```bash
# Custom resolution
sudo ./build/camera_to_matrix --width 1280 --height 720

# Chain of 64x64 matrices
sudo ./build/camera_to_matrix --rows 64 --cols 64 --chain 2

# Single 64x64 matrix (default)
sudo ./build/camera_to_matrix

# Adafruit HAT
sudo ./build/camera_to_matrix --hardware-mapping adafruit-hat
```

### rpicam_to_matrix (rpicam-vid Pipeline)

This version reads raw RGB888 frames from stdin, allowing you to use `rpicam-vid` with FFmpeg for format conversion. Useful for comparing different capture methods.

#### Basic Usage

```bash
# Using rpicam-vid with FFmpeg conversion
rpicam-vid -t 0 --width 640 --height 480 --codec yuv420 -o - | \
  ffmpeg -loglevel error -f rawvideo -pix_fmt yuv420p -s 640x480 -r 30 -i - \
    -f rawvideo -pix_fmt rgb24 - | \
    sudo ./build/rpicam_to_matrix --width 640 --height 480
```

#### Command-line Options

```bash
sudo ./build/rpicam_to_matrix [options]

Options:
  --width WIDTH          Input video width (default: 640)
  --height HEIGHT        Input video height (default: 480)
  --rows ROWS            Matrix rows per panel (default: 64)
  --cols COLS            Matrix columns per panel (default: 64)
  --chain CHAIN          Number of chained matrices (default: 1)
  --parallel PARALLEL    Number of parallel chains (default: 1)
  --hardware-mapping MAP Hardware mapping: regular, adafruit-hat, adafruit-hat-pwm (default: regular)
  --help                 Show help message
```

#### Examples

```bash
# Basic rpicam-vid pipeline
rpicam-vid -t 0 --width 640 --height 480 --codec yuv420 -o - | \
  ffmpeg -loglevel error -f rawvideo -pix_fmt yuv420p -s 640x480 -r 30 -i - \
    -f rawvideo -pix_fmt rgb24 - | \
    sudo ./build/rpicam_to_matrix --width 640 --height 480

# Using H264 codec with inline headers
rpicam-vid -t 0 --width 640 --height 480 --codec h264 --inline -o - | \
  ffmpeg -loglevel error -f h264 -i - -c:v copy -f mpegts - | \
    ffmpeg -loglevel error -f mpegts -i - -f rawvideo -pix_fmt rgb24 - | \
    sudo ./build/rpicam_to_matrix --width 640 --height 480

# Custom matrix configuration
rpicam-vid -t 0 --width 1280 --height 720 --codec yuv420 -o - | \
  ffmpeg -loglevel error -f rawvideo -pix_fmt yuv420p -s 1280x720 -r 30 -i - \
    -f rawvideo -pix_fmt rgb24 - | \
    sudo ./build/rpicam_to_matrix --width 1280 --height 720 --rows 64 --cols 64
```

**Note**: The `rpicam_to_matrix` executable displays a test pattern on startup to verify the matrix is working before waiting for input.

## Configuration

### Matrix Configuration

The matrix configuration depends on your hardware setup:

- **Single 64x64 matrix** (default): `--rows 64 --cols 64 --chain 1`
- **Chain of 64x64 matrices**: `--rows 64 --cols 64 --chain 2`
- **32x32 matrix**: `--rows 32 --cols 32 --chain 1`
- **Multiple parallel chains**: `--parallel 2` (for 2 parallel chains)

**Note**: Both executables use the same matrix configuration:
- GPIO slowdown: 4 (`--led-slowdown-gpio=4`)
- Hardware pulsing disabled (`--led-no-hardware-pulse`)
- Brightness: 50%

### Hardware Mapping

- `regular`: Standard GPIO pin mapping (hand-wired or custom adapter)
- `adafruit-hat`: Adafruit RGB Matrix HAT/Bonnet pinout
- `adafruit-hat-pwm`: Adafruit HAT with PWM hardware modification

See the [rpi-rgb-led-matrix documentation](https://github.com/hzeller/rpi-rgb-led-matrix) for more details on hardware configuration.

### Camera Settings

The default resolution (640x480) works well for most LED matrices. Higher resolutions will be downscaled to fit the matrix size. Lower resolutions may improve performance.

### Field of View (FOV) Control

The `--sensor-width` and `--sensor-height` flags allow you to control the camera's field of view independently from processing resolution.

#### How It Works

**Important:** Libcamera's sensor mode selection is coupled to requested stream size. This creates a fundamental tradeoff:

```
Small Output Request (e.g., 576x192)
  → Libcamera selects small sensor mode (e.g., 1536x864)
  → Narrower field of view (zoomed in)
  → Fast processing

Large Sensor Request (e.g., 2304x1296)
  → Libcamera selects larger sensor mode (e.g., 2304x1296)  
  → Wider field of view
  → Slower due to software downscaling overhead
```

#### Usage

```bash
# Wide FOV - uses 2304x1296 sensor mode, scales to 576x192 for processing
sudo ./build/camera_to_matrix --width 576 --height 192 \
  --sensor-width 2304 --sensor-height 1296 --led-chain 3

# Very wide FOV - uses full sensor, but MUCH slower
sudo ./build/camera_to_matrix --width 576 --height 192 \
  --sensor-width 4608 --sensor-height 2592 --led-chain 3
```

#### Performance Impact

**The tradeoff is unavoidable with current implementation:**

- **Wider FOV = Lower FPS** (more data to transfer and scale)
- **Narrower FOV = Higher FPS** (less data to process)

Typical performance on Raspberry Pi 4:

| Configuration | Mode 1 (Pass-through) | Modes 2-5 (Effects) | FOV |
|---------------|----------------------|---------------------|-----|
| Auto (no sensor flags) | ~55 fps | ~39 fps | Medium (cropped) |
| `--sensor 2304x1296` | ~36 fps | ~25 fps | Wide |
| `--sensor 4608x2592` | ~20 fps | ~14 fps | Maximum |

#### When to Use

✅ **Use sensor mode flags when:**
- You need wider field of view than auto-selection provides
- You can accept lower FPS (25-35fps is enough)
- You're using lower processing resolutions (e.g., 384x128)

❌ **Don't use sensor flags when:**
- You need maximum FPS (>50fps)
- The auto-selected FOV is acceptable
- You're already bottlenecked on effects processing

#### Viewing Available Sensor Modes

Check what sensor modes your camera supports:

```bash
libcamera-hello --list-cameras
```

Typical Camera Module 3 (IMX708) modes:
- `4608x2592` @ 14fps - Full sensor (widest FOV, slowest)
- `2304x1296` @ 56fps - Half resolution (wide FOV, moderate speed)
- `1536x864` @ 120fps - Cropped/binned (medium FOV, fastest)

#### Why Isn't the ISP Scaling for Free?

**The fundamental limitation:** Libcamera's public API couples sensor mode selection to requested output stream size. While the Raspberry Pi's ISP hardware CAN scale efficiently, libcamera doesn't expose fine-grained control to:
1. Force a specific sensor mode
2. AND request a different output size  
3. With ISP-only (hardware) scaling

The current implementation requests a large stream size (forces wide sensor mode), receives the large frames from the camera, then uses software `cv::resize()` to downscale. This CPU overhead is why wider FOV hurts performance.

#### Alternatives

If you need both wide FOV AND high FPS:

1. **Lower Processing Resolution**
   ```bash
   # Process at 384x128 instead of 576x192
   sudo ./build/camera_to_matrix --width 384 --height 128 \
     --sensor-width 2304 --sensor-height 1296 --led-chain 3
   ```
   You may still get ~45fps with acceptable detail.

2. **Physical Camera Changes**
   - Use a different lens (wide-angle lens for Camera Module 3)
   - Physically position the camera further from the scene
   - Use a different camera with wider default FOV

3. **Accept Auto FOV**
   ```bash
   # Let libcamera pick - usually 1536x864 mode with decent FOV
   sudo ./build/camera_to_matrix --width 576 --height 192 --led-chain 3
   ```
   Fast and often good enough.

4. **Future Improvement**
   The ideal solution would use libcamera's raw stream API or a lower-level camera interface (`v4l2`, `MMAL`) to decouple sensor mode selection from output scaling. This is complex and outside the scope of the current implementation.

#### Summary

The `--sensor-width/height` feature **does work** and provides wider FOV, but at a **significant performance cost** due to software scaling. Use it when FOV is more important than FPS, or reduce your processing resolution to compensate.

## Installation

To install the executable system-wide:

```bash
sudo make install
```

This installs `camera_to_matrix` to `/usr/local/bin`, allowing you to run it from anywhere:

```bash
sudo camera_to_matrix
```

## Troubleshooting

### Camera Not Found
- Ensure the camera is properly connected and enabled: `sudo raspi-config` → Interface Options → Camera → Enable
- Check camera detection: `libcamera-hello --list-cameras`
- Test camera: `libcamera-hello`

### Matrix Not Displaying
- Ensure you're running with `sudo` (required for GPIO access)
- Check GPIO pin connections
- Verify matrix configuration matches your hardware (rows, cols, chain length)
- Try reducing brightness in code if colors appear washed out

### Build Errors
- **rpi-rgb-led-matrix not found**: Ensure it's cloned and built in the project directory
- **libcamera not found**: Install with `sudo apt install libcamera-dev`
- **cmake not found**: Install with `sudo apt install cmake`

### Performance Issues
- Reduce camera resolution: `--width 320 --height 240`
- Check CPU usage: `top` or `htop`
- Consider using `isolcpus` kernel parameter to reserve a CPU core (see rpi-rgb-led-matrix docs)

### Flickering
- If using Adafruit HAT, consider the PWM hardware modification
- Check power supply - LED matrices require significant power
- Ensure adequate cooling

## Development

### Building from Source

```bash
# Clean build
make clean
make
```

### Code Structure

- `src/camera_to_matrix.cpp`: Direct libcamera capture
  - `CameraToMatrix` class: Handles camera and matrix initialization
  - `run()`: Main capture and display loop
  - `displayFrame()`: Converts and displays frames on matrix

- `src/rpicam_to_matrix.cpp`: Reads rpicam-vid output from stdin
  - `RpicamToMatrix` class: Handles matrix initialization and stdin reading
  - `run()`: Reads raw RGB888 frames from stdin and displays them
  - `displayFrame()`: Converts and displays frames on matrix
  - `displayTestPattern()`: Shows a test pattern on startup

### Adding Features

To add new features:
1. Modify `src/app/app_core.cpp` (shared effects/modes)
2. Rebuild: `make clean && make`
3. Test with your hardware

## License

This project is open source. Please check individual component licenses:
- rpi-rgb-led-matrix: GPL-2.0
- Your code: (specify your license)

## References

- [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix) - RGB LED matrix control library
- [Raspberry Pi Camera Documentation](https://www.raspberrypi.com/documentation/computers/camera_software.html)
- [libcamera Documentation](https://libcamera.org/)

## Support

For issues related to:
- **LED Matrix Hardware**: See [rpi-rgb-led-matrix issues](https://github.com/hzeller/rpi-rgb-led-matrix/issues)
- **Camera Hardware**: See [Raspberry Pi Forums](https://forums.raspberrypi.com/)
- **This Project**: Open an issue in this repository
