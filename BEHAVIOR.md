# Effect Behavior Reference

## Effect Summary Table

| Effect/Shortcut | Name | Description (Expected Visual) | Processing (Library Mechanisms) |
|-----------------|------|-------------------------------|--------------------------------|
| `1` | Debug View (Pass-through) | Unmodified camera feed displayed directly | `cv::Mat` shallow copy (`out_bgr = in_bgr`) |
| `2` | Filled Silhouette | White filled silhouettes of detected movement on black background | `cv::createBackgroundSubtractorMOG2()` → foreground mask → `cv::findContours()` → `cv::drawContours()` with `FILLED` |
| `3` | Outline Only | White wireframe outlines of detected movement on black background | `cv::createBackgroundSubtractorMOG2()` → foreground mask → `cv::findContours()` → `cv::drawContours()` with thickness=2 |
| `4` | Motion Trails | Fading ghost trails showing movement history, white silhouettes that decay over time | `cv::createBackgroundSubtractorMOG2()` → foreground mask → `cv::findContours()` → frame multiplication (`silhouette_frame_ *= trail_alpha_`) → `cv::drawContours()` with `FILLED` |
| `5` | Rainbow Motion Trails | Camera feed with colorful rainbow trails overlaid on detected movement paths | `cv::createBackgroundSubtractorMOG2()` → morphological operations (`cv::morphologyEx()` OPEN/CLOSE) → `cv::findContours()` → float trail age buffer (`CV_32FC1`) → HSV color space conversion → per-pixel hue mapping based on position/time → `cv::cvtColor()` HSV→BGR → alpha blending with camera feed using inverse mask |
| `6` | Double Exposure | Camera feed blended with past frames (ghost effect), randomized time offsets | Frame history buffer (circular) → `cv::createBackgroundSubtractorMOG2()` → `cv::GaussianBlur()` → `cv::addWeighted()` (25% current, 75% past) → `cv::copyTo()` with mask |
| `7` | Procedural Shapes | Animated tessellated geometric shapes (circles, triangles, squares, hexagons, stars) morphing and scrolling diagonally | Procedural generation → shape morphing interpolation → hexagonal/square tiling → HSV color space (`hsvToBgr()` custom function) → `cv::fillPoly()` / `cv::polylines()` → diagonal scroll with wrap-around |
| `8` | Wave Patterns | Dynamic interference patterns with multiple sine waves, shifting colors and brightness | Multi-wave interference calculation (sine waves) → half-resolution processing → HSV color mapping based on position and time → `cv::resize()` upscaling → per-pixel rendering |
| `9` | Geometric Abstraction | Low-poly geometric interpretation of detected movement, blocky polygonal aesthetic | `cv::createBackgroundSubtractorMOG2()` → morphological operations → `cv::findContours()` → `cv::approxPolyDP()` (polygon approximation) → HSV color based on contour area → `cv::fillPoly()` / `cv::polylines()` |

## Additional Controls

| Control | Shortcut | Description |
|---------|----------|-------------|
| Debug Info | `d` | Toggle FPS and CPU temperature overlay |
| Panel Layout | `q` | Toggle between EXTEND (image spans panels) and REPEAT (same image, different effects per panel) |
| Multi-Panel Mode | `§` | Toggle multi-panel mode and cycle target (P1 → P2 → ... → All → Off) |
| Auto-Cycling | `a` | Toggle automatic effect cycling (3-7 second intervals) |

## Processing Details

### Background Subtraction
- **Algorithm**: MOG2 (Mixture of Gaussians)
- **Parameters**: History=500, Threshold=16, Shadow detection enabled
- **Output**: Binary foreground mask (`CV_8UC1`)
- **Usage**: Effects 2, 3, 4, 5, 6, 9

### Morphological Operations
- **Operations**: `cv::MORPH_OPEN` (remove noise) → `cv::MORPH_CLOSE` (fill holes)
- **Kernel**: Elliptical, Size(5,5)
- **Usage**: Effects 5, 9 (noise reduction)

### Contour Detection
- **Method**: `cv::findContours()` with `RETR_EXTERNAL` and `CHAIN_APPROX_SIMPLE`
- **Filtering**: Minimum contour area threshold (1000 pixels for effects 2-4, 9; 1500 for effect 5)
- **Usage**: Effects 2, 3, 4, 5, 6, 9

### Color Space Conversions
- **BGR ↔ HSV**: Used in effects 5, 7, 8, 9
- **HSV Range**: Hue 0-360 (custom hsvToBgr function) or 0-180 (OpenCV convention), Saturation 0-255, Value 0-255
- **Custom HSV→BGR**: Manual conversion function for effects 7, 8, 9

### Frame Blending
- **Alpha Blending**: `cv::addWeighted()` for transitions and double exposure
- **Mask-based Copying**: `cv::copyTo()` with binary masks for selective overlay
- **Per-pixel Alpha**: Manual alpha blending in effect 5
- **Usage**: Effects 4, 5, 6, transitions between effects

### Frame History
- **Circular Buffer**: Used in effect 6 (Double Exposure)
- **Size**: MAX_FRAME_HISTORY frames
- **Randomized Offsets**: Time offset changes every 60 frames

### Polygon Approximation
- **Method**: `cv::approxPolyDP()` (Douglas-Peucker algorithm)
- **Epsilon**: 15.0 pixels (approximation accuracy)
- **Usage**: Effect 9 (Geometric Abstraction)

### Performance Optimizations
- **Resolution Scaling**: Wave patterns processed at half resolution then upscaled
- **Frame Caching**: Previous frame output cached for smooth transitions
- **Conditional Processing**: Movement detection skipped in ambient mode (every 10 frames)

### Auto-Cycling
- **Interval**: Random between 3-7 seconds
- **Transition**: 30 frames (1 second at 30fps) with alpha blending
- **Logging**: Console output shows effect transitions when auto-cycling is enabled
