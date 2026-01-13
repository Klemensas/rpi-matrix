# Performance Optimizations and Enhancement Guide

This document catalogs all performance optimizations currently applied in the codebase and provides guidance on how to enhance effects for more powerful hardware.

## Current Optimizations Applied

### 1. Resolution Scaling

#### Wave Patterns (Effect 8)
- **Location**: `src/app/app_core.cpp:1040-1077`
- **Optimization**: Process at half resolution (width/2, height/2), then upscale
- **Speedup**: ~4x (processes 1/4 the pixels)
- **Trade-off**: Slight loss of detail in wave patterns
- **Enhancement Path**: 
  - Remove resolution scaling for sharper, more detailed interference patterns
  - Process at full resolution for pixel-perfect wave rendering
  - Add more wave layers (currently 3) for richer interference patterns
  - Use higher-quality upscaling (bicubic instead of linear interpolation)

### 2. Animation Speed Reduction

#### Procedural Shapes (Effect 7)
- **Location**: `src/app/app_core.cpp:331-343`
- **Optimizations Applied**:
  - Color morphing: `0.25f` multiplier (full cycle every 4 seconds) - **half speed**
  - Hue rotation: `5.0f` multiplier - **half speed** (commented as "half speed")
  - Shape morphing: `0.0075f` increment - **half speed** (commented as "half speed")
  - Fill mode cycle: `0.15f` multiplier - **half speed** (commented as "half speed")
- **Speedup**: Reduces CPU usage by processing fewer animation updates
- **Trade-off**: Slower, less dynamic visual transitions
- **Enhancement Path**:
  - Double all animation speeds for more dynamic effects
  - Add more shape types (currently 5: circle, triangle, square, hexagon, star)
  - Increase shape density (currently `size_factor` 0.11-0.14)
  - Add per-shape rotation animations
  - Implement more complex color gradients (currently simple hue interpolation)

### 3. Early Exit Optimizations

#### Procedural Shapes (Effect 7)
- **Location**: `src/app/app_core.cpp:461-471`
- **Optimization**: Skip rendering shapes completely outside visible bounds
- **Speedup**: Avoids unnecessary drawing operations
- **Trade-off**: None (purely beneficial)
- **Enhancement Path**: 
  - Add more complex culling (frustum culling for rotated shapes)
  - Implement level-of-detail (LOD) for distant shapes

### 4. Contour Area Thresholds

#### All Background Subtraction Effects (2, 3, 4, 5, 6, 9)
- **Location**: Various `findPersonContours()` calls
- **Optimizations Applied**:
  - Effect 2-4, 9: `min_contour_area = 1000` pixels
  - Effect 5: `min_contour_area = 1500` pixels (increased to reduce noise)
- **Speedup**: Filters out small noise contours, reducing processing
- **Trade-off**: May miss small movements or fine details
- **Enhancement Path**:
  - Lower thresholds to 500-800 pixels for more sensitive detection
  - Add adaptive thresholding based on scene activity
  - Implement multi-scale contour detection for better detail capture

### 5. Morphological Operations

#### Double Exposure (Effect 6)
- **Location**: `src/app/app_core.cpp:647-649`
- **Optimization**: Minimal cleanup - only `MORPH_CLOSE` with small kernel (3x3)
- **Speedup**: Faster than full morphological pipeline
- **Trade-off**: Less noise reduction, potentially noisier masks
- **Enhancement Path**:
  - Add `MORPH_OPEN` before `MORPH_CLOSE` for better noise removal
  - Increase kernel size to 5x5 or 7x7 for smoother edges
  - Add additional morphological operations for cleaner masks

#### Rainbow Trails (Effect 5)
- **Location**: `src/app/app_core.cpp:211-213`
- **Optimization**: Full morphological pipeline (OPEN + CLOSE) with 5x5 kernel
- **Current State**: Already optimized for quality
- **Enhancement Path**:
  - Increase kernel size to 7x7 or 9x9 for even cleaner detection
  - Add additional passes for very noisy environments

### 6. Per-Pixel Processing Optimizations

#### Rainbow Trails (Effect 5)
- **Location**: `src/app/app_core.cpp:250-312`
- **Optimizations Applied**:
  - Manual per-pixel alpha blending (avoiding OpenCV's slower operations)
  - Threshold check (`intensity > 20`) to skip dark pixels
  - Alpha threshold (`alpha > 0.08f`) to skip very transparent pixels
- **Speedup**: Skips unnecessary blending operations
- **Trade-off**: Manual implementation is less optimized than OpenCV's vectorized operations
- **Enhancement Path**:
  - Use OpenCV's `cv::blendLinear()` or `cv::addWeighted()` with masks for better performance
  - Remove thresholds for more subtle trail effects
  - Add per-pixel color variation (currently uniform hue mapping)

### 7. Frame History Limitations

#### Double Exposure (Effect 6)
- **Location**: `include/app/app_core.h:97-99`
- **Optimizations Applied**:
  - `MAX_FRAME_HISTORY = 90` frames (~3 seconds at 30fps)
  - `MIN_TIME_OFFSET = 15` frames (0.5 seconds)
  - `MAX_TIME_OFFSET = 75` frames (2.5 seconds)
  - Time offset changes every 60 frames (2 seconds)
- **Speedup**: Limited memory usage, faster random offset selection
- **Trade-off**: Shorter history window, less dramatic time shifts
- **Enhancement Path**:
  - Increase `MAX_FRAME_HISTORY` to 180-300 frames (6-10 seconds)
  - Expand time offset range to 30-150 frames (1-5 seconds)
  - Change offset more frequently (every 30 frames) for more dynamic effects
  - Add multiple past frames blending (currently only 1 past frame)

### 8. Transition Frame Caching

#### All Effects (During Transitions)
- **Location**: `src/app/app_core.cpp:88-126`
- **Optimization**: `previous_frame_output_` cached to avoid re-rendering previous effect
- **Speedup**: Avoids double rendering during transitions
- **Trade-off**: Memory usage for cached frame
- **Enhancement Path**:
  - Already optimal - no changes needed
  - Could add multi-frame caching for smoother multi-effect transitions

### 9. Multi-Panel Processing

#### Multi-Panel Mode
- **Location**: `src/app/app_core.cpp:766-768`
- **Optimization**: Resize input to single panel width before processing
- **Speedup**: Processes smaller images per panel
- **Trade-off**: Lower resolution per panel in REPEAT mode
- **Enhancement Path**:
  - Process at full resolution for each panel (better quality)
  - Add per-panel effect-specific optimizations

### 10. Background Subtraction Parameters

#### All Background Subtraction Effects
- **Location**: `src/app/app_core.cpp:16` (constructor)
- **Current Parameters**: `cv::createBackgroundSubtractorMOG2(500, 16, true)`
  - History: 500 frames
  - Threshold: 16
  - Shadow detection: enabled
- **Optimization**: Standard parameters for balance between quality and performance
- **Enhancement Path**:
  - Increase history to 1000+ frames for better background learning
  - Lower threshold to 8-12 for more sensitive detection
  - Add per-effect background subtractor instances with tuned parameters

### 11. Gaussian Blur Size

#### Double Exposure (Effect 6)
- **Location**: `src/app/app_core.cpp:652`
- **Optimization**: `cv::GaussianBlur()` with kernel size 15x15
- **Current State**: Moderate blur for smooth edges
- **Enhancement Path**:
  - Increase to 21x21 or 31x31 for softer, more dreamlike blending
  - Add multiple blur passes for extreme soft-focus effects
  - Use bilateral filter instead for edge-preserving smoothing

### 12. Polygon Approximation Epsilon

#### Geometric Abstraction (Effect 9)
- **Location**: `src/app/app_core.cpp:1103`
- **Optimization**: `epsilon = 15.0` pixels for polygon approximation
- **Speedup**: Fewer polygon vertices = faster rendering
- **Trade-off**: Less geometric detail
- **Enhancement Path**:
  - Lower epsilon to 5.0-10.0 for more detailed polygons
  - Add adaptive epsilon based on contour size
  - Implement multiple approximation levels for LOD rendering

### 13. Trail Decay Rates

#### Motion Trails (Effect 4)
- **Location**: `src/app/app_core.cpp:193`
- **Optimization**: `trail_alpha_ = 0.7f` (30% decay per frame)
- **Current State**: Moderate decay rate
- **Enhancement Path**:
  - Lower to 0.5-0.6 for longer, more dramatic trails
  - Add distance-based decay (trails fade based on distance from current position)

#### Rainbow Trails (Effect 5)
- **Location**: `src/app/app_core.cpp:226`
- **Optimization**: `trail_age_buffer_ *= 0.93f` (7% decay per frame)
- **Current State**: Slow decay for longer trails
- **Enhancement Path**:
  - Lower to 0.88-0.90 for even longer trails
  - Add velocity-based decay (faster movement = brighter trails)

## Enhancement Opportunities by Effect

### Effect 2: Filled Silhouette
**Current Optimizations**: None (already very fast)
**Enhancement Path**:
- Add gradient fills (distance transform)
- Add edge detection for outline highlights
- Add color coding based on movement speed

### Effect 3: Outline Only
**Current Optimizations**: None (already very fast)
**Enhancement Path**:
- Add variable thickness based on movement
- Add animated outline (pulsing, scanning)
- Add color gradients along outline

### Effect 4: Motion Trails
**Current Optimizations**: Frame multiplication for decay
**Enhancement Path**:
- Add color-coded trails based on age
- Add velocity vectors showing movement direction
- Add trail width variation based on speed
- Implement multi-layer trails with different decay rates

### Effect 5: Rainbow Trails
**Current Optimizations**: 
- Morphological operations
- Per-pixel thresholding
- Manual alpha blending
**Enhancement Path**:
- Remove intensity threshold for more subtle trails
- Add more wave layers to hue calculation
- Implement particle-based trail system
- Add trail width variation
- Use OpenCV's optimized blending operations

### Effect 6: Double Exposure
**Current Optimizations**:
- Limited frame history (90 frames)
- Minimal morphological operations
- Fixed time offset range
**Enhancement Path**:
- Increase frame history to 300+ frames
- Add multiple past frames (3-5 frame blending)
- Implement motion-based frame selection
- Add color grading to past frames
- Increase blur kernel for softer blending

### Effect 7: Procedural Shapes
**Current Optimizations**:
- Half-speed animations
- Early exit for off-screen shapes
- Limited shape count
**Enhancement Path**:
- Double all animation speeds
- Add 10+ shape types
- Increase shape density by 2-3x
- Add per-shape rotation and scaling animations
- Implement 3D perspective transforms
- Add particle effects between shapes

### Effect 8: Wave Patterns
**Current Optimizations**:
- Half-resolution processing (4x speedup)
**Enhancement Path**:
- **Remove resolution scaling** - process at full resolution
- Add 5-10 wave layers instead of 3
- Implement 3D wave interference
- Add interactive wave sources (respond to movement)
- Use GPU acceleration for wave calculations
- Add Fourier transform-based wave synthesis

### Effect 9: Geometric Abstraction
**Current Optimizations**:
- Polygon approximation (epsilon = 15.0)
- Morphological operations
**Enhancement Path**:
- Lower epsilon to 5.0-10.0 for more detail
- Add gradient fills within polygons
- Implement Voronoi diagram-based abstraction
- Add 3D extrusion effect
- Add animation to polygon vertices

## Hardware-Specific Recommendations

### For Raspberry Pi 4 (Current Target)
- Keep all current optimizations
- Consider removing half-resolution scaling for Effect 8 if FPS > 30
- Monitor CPU temperature and throttle if needed

### For Raspberry Pi 5 or More Powerful Hardware
1. **Remove Resolution Scaling** (Effect 8)
   - Process at full resolution
   - Add more wave layers (5-10)

2. **Increase Animation Speeds** (Effect 7)
   - Double all animation multipliers
   - Add more shape types

3. **Expand Frame History** (Effect 6)
   - Increase to 300 frames
   - Add multi-frame blending

4. **Enhance Background Subtraction**
   - Increase history to 1000 frames
   - Lower threshold for sensitivity

5. **Improve Polygon Detail** (Effect 9)
   - Lower epsilon to 5.0-10.0
   - Add gradient fills

6. **Remove Per-Pixel Thresholds** (Effect 5)
   - Process all pixels for smoother trails
   - Use OpenCV's optimized operations

### For Desktop/High-End Hardware
- Remove ALL optimizations
- Process at 2x-4x resolution (supersampling)
- Add GPU acceleration where possible
- Implement real-time shader effects
- Add machine learning-based effects

## Performance Monitoring

### Key Metrics to Monitor
1. **Frame Rate**: Target 30 FPS, acceptable 20-30 FPS
2. **CPU Usage**: Monitor per-effect CPU usage
3. **Memory Usage**: Watch for memory leaks in frame history
4. **Temperature**: Raspberry Pi thermal throttling at 80°C

### Profiling Recommendations
- Use `perf` or `gprof` to identify bottlenecks
- Profile each effect individually
- Monitor OpenCV operation timings
- Track memory allocations

## Code Locations for Quick Reference

| Optimization | File | Line Range |
|--------------|------|------------|
| Wave Patterns Resolution Scaling | `src/app/app_core.cpp` | 1040-1077 |
| Procedural Shapes Animation Speed | `src/app/app_core.cpp` | 331-343 |
| Procedural Shapes Early Exit | `src/app/app_core.cpp` | 461-471 |
| Rainbow Trails Per-Pixel Blending | `src/app/app_core.cpp` | 250-312 |
| Double Exposure Frame History | `include/app/app_core.h` | 97-99 |
| Double Exposure Morphology | `src/app/app_core.cpp` | 647-649 |
| Transition Caching | `src/app/app_core.cpp` | 88-126 |
| Multi-Panel Resizing | `src/app/app_core.cpp` | 766-768 |
| Background Subtractor Config | `src/app/app_core.cpp` | 16 |
| Geometric Abstraction Epsilon | `src/app/app_core.cpp` | 1103 |

## Summary

The codebase is well-optimized for Raspberry Pi 4 hardware, with most optimizations focused on:
1. Reducing pixel processing (resolution scaling)
2. Slowing animations (CPU savings)
3. Limiting memory usage (frame history)
4. Early exit optimizations (skipping unnecessary work)

For more powerful hardware, the primary enhancement paths are:
- **Remove resolution scaling** for sharper visuals
- **Increase animation speeds** for more dynamic effects
- **Expand frame history** for longer time-based effects
- **Lower thresholds** for more sensitive detection
- **Add more computational layers** (more waves, more shapes, more frames)

All optimizations are clearly marked in code comments and can be easily adjusted based on target hardware capabilities.
