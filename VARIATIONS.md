# Silhouette Display Variations for LED Matrix Art Installation

## Context

This document describes creative alternatives to the basic filled silhouette display for the rpi-matrix project. The current implementation (`processTransformedFrame()` in `src/camera_to_matrix.cpp`) uses:

- **Background Subtraction**: MOG2 algorithm to detect moving objects (people)
- **Contour Detection**: Finds outlines of detected objects
- **Current Display**: White filled silhouettes on black background
- **Frame Format**: BGR888 from camera, converted to RGB888 for matrix display
- **Matrix Display**: Low-resolution LED matrix (typically 64x64 or chained panels)

## Current Implementation Reference

**Location**: `src/camera_to_matrix.cpp`, `processTransformedFrame()` method

**Current Flow**:
1. Convert raw BGR888 camera data to OpenCV Mat
2. Apply MOG2 background subtraction → `fg_mask` (binary mask)
3. Find contours from foreground mask
4. Filter contours by area (> 1000 pixels)
5. Draw filled white contours on black frame
6. Convert BGR to RGB and display on matrix

**Key Variables**:
- `frame_bgr`: Input frame from camera (BGR format)
- `fg_mask`: Binary mask of detected foreground objects
- `contours`: Vector of detected contour points
- `silhouette_frame_`: Output frame (CV_8UC3, BGR format)
- `min_contour_area`: Currently 1000 pixels (fixed threshold)

## Display Variations

### 1. Outline Only (Wireframe)

**Description**: Draw only the contour edges, creating a minimal wireframe aesthetic.

**Visual Effect**: Clean, architectural lines showing person's shape without fill.

**Implementation**:
```cpp
// Replace FILLED with thickness parameter
cv::drawContours(silhouette_frame_, std::vector<std::vector<cv::Point>>{contour}, 
                -1, cv::Scalar(255, 255, 255), 2);  // 2-pixel thick outline
```

**Parameters**:
- Thickness: 1-5 pixels (adjust for matrix resolution)
- Color: White (255, 255, 255) or customize

**Performance**: Excellent (very fast)

**Best For**: Minimalist installations, architectural aesthetics

---

### 2. Dotted/Dashed Outline

**Description**: Animated dots along the contour path, creating a scanning/tracing effect.

**Visual Effect**: Dots that appear to trace the person's outline, can be animated.

**Implementation**:
```cpp
// Draw points along contour with spacing
for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        // Animate dot position (optional)
        static int dot_offset = 0;
        dot_offset = (dot_offset + 1) % 10;
        
        // Draw every Nth point
        for (size_t i = dot_offset; i < contour.size(); i += 10) {
            cv::circle(silhouette_frame_, contour[i], 2, 
                       cv::Scalar(255, 255, 255), -1);
        }
    }
}
```

**Parameters**:
- Spacing: Points between dots (5-20 recommended)
- Dot size: Radius of circles (1-3 pixels)
- Animation speed: Increment per frame

**Performance**: Excellent

**Best For**: Dynamic, tech-inspired installations

---

### 3. Motion Trails (Ghost Effect)

**Description**: Fade previous frames to create trailing motion history.

**Visual Effect**: Shows movement path as fading trails behind the person.

**Implementation**:
```cpp
// Add to class member variables:
float trail_alpha_ = 0.7;  // Fade factor (0.0-1.0)

// In processTransformedFrame(), before drawing new contours:
// Fade previous frame
silhouette_frame_ *= trail_alpha_;

// Then draw new contours on top (existing code)
for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        cv::drawContours(silhouette_frame_, std::vector<std::vector<cv::Point>>{contour}, 
                        -1, cv::Scalar(255, 255, 255), cv::FILLED);
    }
}
```

**Parameters**:
- `trail_alpha_`: 0.5-0.9 (lower = longer trails, higher = shorter)
- Can vary by distance from current position

**Performance**: Good (requires per-pixel multiplication)

**Best For**: Showing movement patterns, dance installations

---

### 4. Particle Field

**Description**: Convert silhouette to scattered particles/points within the detected area.

**Visual Effect**: Person appears as a cloud of points, abstract representation.

**Implementation**:
```cpp
// Sample random points within contour
const int particles_per_contour = 100;

for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        cv::Rect bbox = cv::boundingRect(contour);
        
        // Generate random points within bounding box
        for (int i = 0; i < particles_per_contour; i++) {
            cv::Point pt(bbox.x + rand() % bbox.width,
                         bbox.y + rand() % bbox.height);
            
            // Check if point is inside contour
            if (cv::pointPolygonTest(contour, pt, false) >= 0) {
                cv::circle(silhouette_frame_, pt, 1, 
                           cv::Scalar(255, 255, 255), -1);
            }
        }
    }
}
```

**Parameters**:
- `particles_per_contour`: 50-200 (more = denser, slower)
- Particle size: 1-2 pixels
- Can make particles move/flow for animation

**Performance**: Moderate (point-in-polygon test is expensive)

**Best For**: Abstract art, particle physics aesthetic

---

### 5. Geometric Abstraction

**Description**: Simplify contour to geometric shapes with fewer points.

**Visual Effect**: Blocky, polygonal, low-poly aesthetic.

**Implementation**:
```cpp
for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        // Approximate contour with fewer points
        std::vector<cv::Point> approx;
        double epsilon = 10.0;  // Approximation accuracy
        cv::approxPolyDP(contour, approx, epsilon, false);
        
        // Draw simplified polygon
        cv::fillPoly(silhouette_frame_, 
                    std::vector<std::vector<cv::Point>>{approx},
                    cv::Scalar(255, 255, 255));
    }
}
```

**Parameters**:
- `epsilon`: 5-20 pixels (higher = more abstract, fewer points)
- Can use different approximation algorithms

**Performance**: Excellent

**Best For**: Retro/8-bit aesthetic, minimalist geometric art

---

### 6. Gradient Fill (Distance-Based)

**Description**: Brightness varies based on distance from edge, creating depth/glow.

**Visual Effect**: Silhouette has gradient from bright center to darker edges.

**Implementation**:
```cpp
// Create distance transform from edge
cv::Mat dist;
cv::distanceTransform(fg_mask, dist, cv::DIST_L2, 5);

// Normalize to 0-255 range
cv::normalize(dist, dist, 0, 255, cv::NORM_MINMAX);

// Convert to 3-channel for display
cv::Mat dist_colored;
cv::applyColorMap(dist, dist_colored, cv::COLORMAP_BONE);  // Grayscale gradient
// Or use: cv::COLORMAP_HOT, cv::COLORMAP_COOL, etc.

// Copy to silhouette frame
dist_colored.copyTo(silhouette_frame_);
```

**Parameters**:
- Distance transform method: DIST_L2, DIST_L1, DIST_C
- Color map: Various OpenCV colormaps
- Can invert for edge-bright effect

**Performance**: Moderate (distance transform is computationally expensive)

**Best For**: Adding depth, glow effects, sophisticated visuals

---

### 7. Scanline Effect

**Description**: Horizontal lines that fill the silhouette, animated top-to-bottom.

**Visual Effect**: Scanning/tracing effect like old CRT displays or 3D printers.

**Implementation**:
```cpp
// Add static variables for animation
static int scanline_y = 0;
static int scanline_direction = 1;  // 1 = down, -1 = up

// Update scanline position
scanline_y += scanline_direction * 2;  // Speed: 2 pixels per frame
if (scanline_y >= height) {
    scanline_y = 0;  // Reset or reverse direction
    scanline_direction *= -1;
}

// Draw horizontal lines up to scanline
for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        for (int y = 0; y < scanline_y; y++) {
            for (int x = 0; x < width; x++) {
                cv::Point pt(x, y);
                if (cv::pointPolygonTest(contour, pt, false) >= 0) {
                    silhouette_frame_.at<cv::Vec3b>(y, x) = cv::Vec3b(255, 255, 255);
                }
            }
        }
    }
}
```

**Parameters**:
- Scanline speed: Pixels per frame (1-5)
- Direction: Top-to-bottom, bottom-to-top, or bidirectional
- Can use multiple scanlines

**Performance**: Slow (per-pixel point-in-polygon test)

**Best For**: Retro tech aesthetic, 3D printing visualization

---

### 8. Skeleton/Stick Figure

**Description**: Extract skeleton from silhouette using morphological operations.

**Visual Effect**: Minimal line-art representation, stick figure aesthetic.

**Implementation**:
```cpp
// Requires OpenCV contrib module (ximgproc)
#include <opencv2/ximgproc.hpp>

// Apply morphological skeletonization
cv::Mat skeleton;
cv::ximgproc::thinning(fg_mask, skeleton);

// Convert to 3-channel and scale to white
cv::cvtColor(skeleton, silhouette_frame_, cv::COLOR_GRAY2BGR);
silhouette_frame_ *= 255;  // Scale binary to white
```

**Dependencies**: OpenCV contrib module (`opencv-contrib`)

**Parameters**:
- Thinning algorithm: Various options in ximgproc
- Can thicken skeleton lines for visibility

**Performance**: Good

**Best For**: Minimalist line art, abstract human forms

---

### 9. Negative Space Inversion

**Description**: Invert colors - person is black, background is white.

**Visual Effect**: High contrast, reversed silhouette.

**Implementation**:
```cpp
// Fill entire frame white
silhouette_frame_ = cv::Scalar(255, 255, 255);

// Draw person as black
for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        cv::drawContours(silhouette_frame_, 
                        std::vector<std::vector<cv::Point>>{contour}, 
                        -1, cv::Scalar(0, 0, 0), cv::FILLED);  // Black fill
    }
}
```

**Parameters**:
- Background color: White (255, 255, 255) or other colors
- Person color: Black (0, 0, 0) or dark gray

**Performance**: Excellent

**Best For**: High contrast installations, reversed aesthetic

---

### 10. Pixelated/Quantized

**Description**: Reduce resolution further for chunky, pixelated effect.

**Visual Effect**: Retro/8-bit aesthetic, blocky pixels.

**Implementation**:
```cpp
// Downscale then upscale using nearest-neighbor interpolation
cv::Mat small;
int block_size = 8;  // Pixelation factor
cv::resize(fg_mask, small, 
           cv::Size(width/block_size, height/block_size), 
           0, 0, cv::INTER_NEAREST);

// Upscale back to original size
cv::resize(small, silhouette_frame_, 
           cv::Size(width, height), 
           0, 0, cv::INTER_NEAREST);

// Convert to 3-channel and scale to white
cv::cvtColor(silhouette_frame_, silhouette_frame_, cv::COLOR_GRAY2BGR);
silhouette_frame_ *= 255;
```

**Parameters**:
- `block_size`: 4-16 (larger = more pixelated)
- Interpolation: INTER_NEAREST for hard edges

**Performance**: Excellent

**Best For**: Retro gaming aesthetic, 8-bit art style

---

### 11. Pulsing/Animated Outline

**Description**: Outline that pulses or animates in width.

**Visual Effect**: Breathing, pulsing effect around person.

**Implementation**:
```cpp
// Add static variables for animation
static int pulse_width = 1;
static bool growing = true;

// Update pulse width
pulse_width += growing ? 1 : -1;
if (pulse_width > 5) growing = false;
if (pulse_width < 1) growing = true;

// Draw with animated width
for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        cv::drawContours(silhouette_frame_, 
                        std::vector<std::vector<cv::Point>>{contour}, 
                        -1, cv::Scalar(255, 255, 255), pulse_width);
    }
}
```

**Parameters**:
- Min/max width: 1-10 pixels
- Speed: Increment per frame
- Can sync with audio or movement

**Performance**: Excellent

**Best For**: Dynamic, living art installations

---

### 12. Color-Coded by Motion Speed

**Description**: Color based on movement velocity.

**Visual Effect**: Shows activity/intensity through color.

**Implementation**:
```cpp
// Track previous centroid positions
static std::map<int, cv::Point> prev_centroids;
static int contour_id = 0;

for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        // Calculate centroid
        cv::Moments m = cv::moments(contour);
        cv::Point centroid(m.m10/m.m00, m.m01/m.m00);
        
        // Calculate speed (distance from previous position)
        double speed = 0.0;
        if (prev_centroids.find(contour_id) != prev_centroids.end()) {
            cv::Point prev = prev_centroids[contour_id];
            speed = cv::norm(centroid - prev);
        }
        prev_centroids[contour_id] = centroid;
        
        // Map speed to color (slow=blue, fast=red)
        cv::Scalar color;
        if (speed < 5) {
            color = cv::Scalar(255, 0, 0);  // Blue (slow)
        } else if (speed < 20) {
            color = cv::Scalar(0, 255, 0);  // Green (medium)
        } else {
            color = cv::Scalar(0, 0, 255);  // Red (fast)
        }
        
        cv::drawContours(silhouette_frame_, 
                        std::vector<std::vector<cv::Point>>{contour}, 
                        -1, color, cv::FILLED);
        contour_id++;
    }
}
```

**Parameters**:
- Speed thresholds: Adjust based on typical movement
- Color mapping: Various schemes (heat map, rainbow, etc.)
- Note: Requires color LED matrix

**Performance**: Good

**Best For**: Interactive installations showing activity levels

---

### 13. Fragmented/Glitch Effect

**Description**: Break silhouette into fragments with random offsets.

**Visual Effect**: Digital glitch, broken/distorted aesthetic.

**Implementation**:
```cpp
for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        // Split contour into segments
        int segment_size = 20;
        for (size_t i = 0; i < contour.size(); i += segment_size) {
            std::vector<cv::Point> segment(
                contour.begin() + i, 
                contour.begin() + std::min(i + segment_size, contour.size())
            );
            
            // Apply random offset to each segment
            int offset_x = (rand() % 10) - 5;  // -5 to +5 pixels
            int offset_y = (rand() % 10) - 5;
            
            for (auto& pt : segment) {
                pt.x += offset_x;
                pt.y += offset_y;
            }
            
            // Draw segment
            cv::fillPoly(silhouette_frame_, 
                        std::vector<std::vector<cv::Point>>{segment},
                        cv::Scalar(255, 255, 255));
        }
    }
}
```

**Parameters**:
- Segment size: 10-30 points
- Offset range: Adjust for glitch intensity
- Can vary offset per segment

**Performance**: Good

**Best For**: Digital art, glitch aesthetic, experimental installations

---

### 14. Radial Rays from Center

**Description**: Draw lines radiating from silhouette center outward.

**Visual Effect**: Sunburst/explosion effect.

**Implementation**:
```cpp
for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > min_contour_area) {
        // Calculate centroid
        cv::Moments m = cv::moments(contour);
        cv::Point center(m.m10/m.m00, m.m01/m.m00);
        
        // Draw rays from center
        int num_rays = 36;  // Number of rays
        int ray_length = std::max(width, height);  // Full frame length
        
        for (int i = 0; i < num_rays; i++) {
            double angle = (i * 360.0 / num_rays) * CV_PI / 180.0;
            cv::Point end(
                center.x + ray_length * cos(angle),
                center.y + ray_length * sin(angle)
            );
            cv::line(silhouette_frame_, center, end, 
                     cv::Scalar(255, 255, 255), 1);
        }
    }
}
```

**Parameters**:
- Number of rays: 12-72
- Ray length: Can be fixed or extend to edge
- Can animate rotation

**Performance**: Excellent

**Best For**: Energy/explosion effects, dynamic visualizations

---

### 15. Morphing Blobs

**Description**: Smooth, organic blob shapes using blur.

**Visual Effect**: Abstract, organic representation.

**Implementation**:
```cpp
// Apply Gaussian blur for smoothness
cv::Mat blurred_mask;
cv::GaussianBlur(fg_mask, blurred_mask, cv::Size(15, 15), 0);

// Threshold to get smooth edges
cv::Mat smooth_mask;
cv::threshold(blurred_mask, smooth_mask, 127, 255, cv::THRESH_BINARY);

// Convert to 3-channel
cv::cvtColor(smooth_mask, silhouette_frame_, cv::COLOR_GRAY2BGR);
silhouette_frame_ *= 255;
```

**Parameters**:
- Blur kernel size: 5-25 (larger = smoother)
- Threshold value: 100-150 (adjust for sensitivity)

**Performance**: Good (blur is moderately expensive)

**Best For**: Abstract art, organic forms, fluid aesthetics

---

## Implementation Guide

### Adding a New Variation

1. **Choose variation** from list above
2. **Add mode number** to keyboard handler (extend switch statement)
3. **Implement variation** in `processTransformedFrame()` or create separate method
4. **Test performance** on Raspberry Pi hardware
5. **Adjust parameters** for desired visual effect

### Code Structure

Current code structure in `src/camera_to_matrix.cpp`:

```cpp
void processTransformedFrame(uint8_t *data, int width, int height) {
    // 1. Convert to OpenCV Mat
    cv::Mat frame_bgr(height, width, CV_8UC3, data);
    
    // 2. Background subtraction
    cv::Mat fg_mask;
    background_subtractor_->apply(frame_bgr, fg_mask);
    
    // 3. Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(fg_mask, contours, ...);
    
    // 4. Initialize output frame
    silhouette_frame_ = cv::Mat::zeros(height, width, CV_8UC3);
    
    // 5. [INSERT VARIATION CODE HERE]
    
    // 6. Convert and display
    cv::Mat silhouette_rgb;
    cv::cvtColor(silhouette_frame_, silhouette_rgb, cv::COLOR_BGR2RGB);
    matrix_.displayFrame(silhouette_rgb.data, width, height);
}
```

### Performance Considerations

**Fast Variations** (suitable for real-time):
- Outline Only (#1)
- Dotted Outline (#2)
- Geometric Abstraction (#5)
- Negative Space (#9)
- Pixelated (#10)
- Pulsing Outline (#11)
- Radial Rays (#14)

**Moderate Performance**:
- Motion Trails (#3)
- Particle Field (#4) - depends on particle count
- Gradient Fill (#6)
- Skeleton (#8)
- Color-Coded Motion (#12)
- Fragmented (#13)
- Morphing Blobs (#15)

**Slow Variations** (may need optimization):
- Scanline Effect (#7) - per-pixel point-in-polygon is expensive

### Optimization Tips

1. **Reduce resolution**: Process at lower resolution, upscale for display
2. **Skip frames**: Process every Nth frame for expensive operations
3. **Limit contours**: Only process largest N contours
4. **Pre-allocate buffers**: Reuse Mat objects instead of creating new ones
5. **Use ROI**: Process only regions of interest

### Combining Variations

Variations can be combined for more complex effects:
- **Outline + Particles**: Wireframe with particle fill
- **Gradient + Motion Trails**: Glowing trails
- **Geometric + Pixelated**: Blocky low-poly aesthetic
- **Negative Space + Scanline**: Inverted scanline effect

### Integration with Mode Switching

Current mode switching uses keys `1` and `2`. To add more variations:

```cpp
// In checkKeyboardInput():
if (key == '3') {
    display_mode_ = 3;  // New variation
    std::cout << "Switched to mode 3: [Variation Name]" << std::endl;
}
// ... etc for modes 4, 5, etc.

// In processFrame():
switch (mode) {
    case 1: /* default */ break;
    case 2: processTransformedFrame(...); break;
    case 3: processVariation3(...); break;  // New method
    // ...
}
```

## Recommended Combinations for Art Installation

1. **Motion Trails + Outline**: Shows movement with clean lines
2. **Gradient Fill + Particles**: Depth with texture
3. **Negative Space + Geometric**: High contrast abstraction
4. **Skeleton + Pulsing**: Minimalist with life
5. **Pixelated + Scanline**: Retro tech aesthetic

## Notes

- All variations assume BGR input format from camera
- Final output must be converted to RGB for matrix display
- Matrix resolution is typically low (64x64), so effects should be visible at that scale
- Some variations may require OpenCV contrib modules
- Performance testing on actual Raspberry Pi hardware is recommended
- Consider adding configuration file for easy parameter tuning
