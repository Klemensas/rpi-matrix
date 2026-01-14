#include "effects/ambient/procedural_shapes.h"
#include <cmath>
#include <opencv2/imgproc.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ProceduralShapesEffect::ProceduralShapesEffect(int width, int height)
    : width_(width), height_(height) {
    reset();
}

void ProceduralShapesEffect::reset() {
    procedural_frame_counter_ = 0;
    procedural_time_ = 0.0f;
    current_shape_type_ = 0;  // 0=circle, 1=triangle, 2=square, 3=hexagon, 4=star
    shape_morph_progress_ = 0.0f;
    hue_shift_ = 0.0f;
    fill_mode_progress_ = 0.0f;  // 0.0 = outline only, 1.0 = filled
    color_morph_progress_ = 0.0f;  // For color morphing
}

void ProceduralShapesEffect::process(cv::Mat& out_bgr, int target_width, int target_height) {
    // Use target dimensions if provided, otherwise use default dimensions
    int output_width = (target_width > 0) ? target_width : width_;
    int output_height = (target_height > 0) ? target_height : height_;

    // Initialize output with black background
    out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);

    procedural_frame_counter_++;
    procedural_time_ = procedural_frame_counter_ * 0.016f;  // ~30fps

    // Slowly morph colors over time (slowed by half)
    color_morph_progress_ = fmod(procedural_time_ * 0.25f, 1.0f);  // Full cycle every 4 seconds
    float base_hue = fmod(procedural_time_ * 5.0f, 360.0f);  // Slow hue rotation (half speed)

    // Morph between shapes (slowed by half)
    if (shape_morph_progress_ >= 1.0f) {
        current_shape_type_ = (current_shape_type_ + 1) % 5;
        shape_morph_progress_ = 0.0f;
    }
    shape_morph_progress_ = std::min(1.0f, shape_morph_progress_ + 0.0075f);  // Half speed

    // Alternate between filled and outline-only (slow cycle, half speed)
    fill_mode_progress_ = 0.5f + 0.5f * std::sin(procedural_time_ * 0.15f);  // 0.0 to 1.0

    // Diagonal scroll speed (slow, linear movement)
    float scroll_speed = 0.8f;  // pixels per frame
    float scroll_x = fmod(procedural_time_ * scroll_speed * 30.0f, width_);
    float scroll_y = fmod(procedural_time_ * scroll_speed * 30.0f, height_);

    // Get tessellation parameters for current and next shape
    int current_shape = current_shape_type_;
    int next_shape = (current_shape + 1) % 5;

    // Helper function to get tessellation params for a shape
    auto getTessellationParams = [this](int shape_type) -> std::pair<float, bool> {
        float size_factor;
        bool hex_tiling;

        switch (shape_type) {
            case 0: // Circle
                size_factor = 0.12f;
                hex_tiling = true;
                break;
            case 1: // Triangle
                size_factor = 0.14f;
                hex_tiling = true;
                break;
            case 2: // Square
                size_factor = 0.11f;
                hex_tiling = false;
                break;
            case 3: // Hexagon
                size_factor = 0.13f;
                hex_tiling = true;
                break;
            case 4: // Star
                size_factor = 0.12f;
                hex_tiling = false;
                break;
            default:
                size_factor = 0.12f;
                hex_tiling = true;
        }
        return std::make_pair(size_factor, hex_tiling);
    };

    // Get parameters for current and next shape
    auto [current_size_factor, current_hex] = getTessellationParams(current_shape);
    auto [next_size_factor, next_hex] = getTessellationParams(next_shape);

    // Interpolate between current and next tessellation parameters
    float size_factor = current_size_factor + (next_size_factor - current_size_factor) * shape_morph_progress_;
    float shape_size = std::min(width_, height_) * size_factor;

    // Interpolate hex tiling (0.0 = square grid, 1.0 = hex grid)
    float hex_tiling_factor = current_hex ? (1.0f - shape_morph_progress_) : shape_morph_progress_;
    if (current_hex && next_hex) hex_tiling_factor = 1.0f;
    if (!current_hex && !next_hex) hex_tiling_factor = 0.0f;

    // Calculate radius with 1-pixel padding between shapes
    // For hex tiling: radius = (shape_size - 1) / 2, for square: radius = (shape_size - 1) / 2
    float radius_factor = (hex_tiling_factor > 0.5f) ? 0.5f : 0.5f;
    int radius = (int)((shape_size - 1.0f) * radius_factor);  // -1 for 1-pixel padding

    // Calculate grid dimensions - ensure we have enough to fill the screen
    int cols = (int)(width_ / shape_size) + 4;

    // Calculate rows needed to fill height completely
    // Account for hex offset which can create gaps
    float row_spacing = shape_size;
    if (hex_tiling_factor > 0.5f) {
        // Hex tiling: vertical spacing is shape_size * sqrt(3)/2, but we use shape_size for simplicity
        // Add extra rows to compensate for hex offset
        row_spacing = shape_size * 0.866f;  // sqrt(3)/2 approximation
    }

    int base_rows = (int)(height_ / row_spacing);
    // Add extra rows to ensure bottom is filled (account for gaps)
    int extra_rows = std::max(2, (int)((height_ - base_rows * row_spacing) / row_spacing) + 2);
    int rows = base_rows + extra_rows + 4;  // Extra padding for scrolling

    for (int row = -1; row < rows; row++) {
        for (int col = -1; col < cols; col++) {
            // Calculate base position for current shape tessellation
            float current_base_x = col * (std::min(width_, height_) * current_size_factor) +
                                   (std::min(width_, height_) * current_size_factor) / 2.0f;
            float current_base_y = row * (std::min(width_, height_) * current_size_factor) +
                                   (std::min(width_, height_) * current_size_factor) / 2.0f;

            // Apply hexagonal offset for current shape
            if (current_hex && row % 2 == 1) {
                current_base_x += (std::min(width_, height_) * current_size_factor) * 0.5f;
            }

            // Calculate base position for next shape tessellation
            float next_base_x = col * (std::min(width_, height_) * next_size_factor) +
                                (std::min(width_, height_) * next_size_factor) / 2.0f;
            float next_base_y = row * (std::min(width_, height_) * next_size_factor) +
                                (std::min(width_, height_) * next_size_factor) / 2.0f;

            // Apply hexagonal offset for next shape
            if (next_hex && row % 2 == 1) {
                next_base_x += (std::min(width_, height_) * next_size_factor) * 0.5f;
            }

            // Interpolate between current and next positions for smooth transition
            float base_x = current_base_x + (next_base_x - current_base_x) * shape_morph_progress_;
            float base_y = current_base_y + (next_base_y - current_base_y) * shape_morph_progress_;

            // Apply smooth linear diagonal scroll
            float center_x = base_x - scroll_x;
            float center_y = base_y - scroll_y;

            // Wrap around for infinite scroll
            float wrap_size = shape_size * 2.0f;
            while (center_x < -wrap_size) center_x += width_ + wrap_size * 2;
            while (center_x > width_ + wrap_size) center_x -= width_ + wrap_size * 2;
            while (center_y < -wrap_size) center_y += height_ + wrap_size * 2;
            while (center_y > height_ + wrap_size) center_y -= height_ + wrap_size * 2;

            // Skip if completely outside visible bounds (with some margin for wrapping)
            if (center_x < -radius - 5 || center_x > width_ + radius + 5 ||
                center_y < -radius - 5 || center_y > height_ + radius + 5) {
                continue;
            }

            // Only draw if shape is at least partially visible
            if (center_x + radius < 0 || center_x - radius > width_ ||
                center_y + radius < 0 || center_y - radius > height_) {
                continue;
            }

            // Color morphing: interpolate between two hues based on position and time
            float hue1 = fmod(base_hue + (row * 25.0f) + (col * 18.0f), 360.0f);
            float hue2 = fmod(base_hue + 120.0f + (row * 25.0f) + (col * 18.0f), 360.0f);
            float current_hue = hue1 + (hue2 - hue1) * color_morph_progress_;
            if (current_hue < 0) current_hue += 360.0f;
            if (current_hue >= 360.0f) current_hue -= 360.0f;

            // Slightly vary saturation and value for visual interest
            float sat = 0.85f + 0.1f * std::sin(procedural_time_ * 0.4f + row + col);
            float val = 0.9f + 0.1f * std::cos(procedural_time_ * 0.3f + row - col);
            cv::Scalar color = hsvToBgr(current_hue, sat, val);

            // Draw shape with morphing and fill/outline mode
            drawMorphingShape(out_bgr, (int)center_x, (int)center_y, radius,
                             current_shape_type_, shape_morph_progress_,
                             color, fill_mode_progress_);
        }
    }
}

// Helper function to convert HSV to BGR
cv::Scalar ProceduralShapesEffect::hsvToBgr(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - std::abs(fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h < 60) {
        r = c; g = x; b = 0;
    } else if (h < 120) {
        r = x; g = c; b = 0;
    } else if (h < 180) {
        r = 0; g = c; b = x;
    } else if (h < 240) {
        r = 0; g = x; b = c;
    } else if (h < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }

    return cv::Scalar((b + m) * 255, (g + m) * 255, (r + m) * 255);
}

// Helper function to draw morphing shapes
void ProceduralShapesEffect::drawMorphingShape(cv::Mat& img, int cx, int cy, int radius,
                                int shape_type, float morph_progress, cv::Scalar color,
                                float fill_mode) {
    std::vector<cv::Point> points;

    // Generate points for current and next shape
    int next_shape = (shape_type + 1) % 5;

    std::vector<cv::Point> current_points = getShapePoints(shape_type, cx, cy, radius);
    std::vector<cv::Point> next_points = getShapePoints(next_shape, cx, cy, radius);

    // Interpolate between shapes
    for (size_t i = 0; i < std::max(current_points.size(), next_points.size()); i++) {
        cv::Point p1 = current_points[i % current_points.size()];
        cv::Point p2 = next_points[i % next_points.size()];
        cv::Point interpolated(
            (int)(p1.x + (p2.x - p1.x) * morph_progress),
            (int)(p1.y + (p2.y - p1.y) * morph_progress)
        );
        points.push_back(interpolated);
    }

    if (points.size() >= 3) {
        // Draw based on fill_mode: 0.0 = outline only, 1.0 = filled
        if (fill_mode > 0.3f) {
            // Draw filled shape
            cv::fillPoly(img, std::vector<std::vector<cv::Point>>{points}, color);
        }

        // Always draw border, but make it more prominent when in outline mode
        int border_thickness = (fill_mode < 0.5f) ? 3 : 2;
        cv::polylines(img, std::vector<std::vector<cv::Point>>{points}, true, color, border_thickness);
    }
}

// Helper to get points for different shape types
std::vector<cv::Point> ProceduralShapesEffect::getShapePoints(int shape_type, int cx, int cy, int radius) {
    std::vector<cv::Point> points;

    switch (shape_type) {
        case 0: { // Circle (approximated with many points)
            int num_points = 32;
            for (int i = 0; i < num_points; i++) {
                float angle = (i * 2.0f * M_PI) / num_points;
                points.push_back(cv::Point(
                    cx + (int)(radius * std::cos(angle)),
                    cy + (int)(radius * std::sin(angle))
                ));
            }
            break;
        }
        case 1: { // Triangle
            for (int i = 0; i < 3; i++) {
                float angle = (i * 2.0f * M_PI / 3.0f) - M_PI / 2.0f;
                points.push_back(cv::Point(
                    cx + (int)(radius * std::cos(angle)),
                    cy + (int)(radius * std::sin(angle))
                ));
            }
            break;
        }
        case 2: { // Square
            for (int i = 0; i < 4; i++) {
                float angle = (i * 2.0f * M_PI / 4.0f) - M_PI / 4.0f;
                points.push_back(cv::Point(
                    cx + (int)(radius * std::cos(angle)),
                    cy + (int)(radius * std::sin(angle))
                ));
            }
            break;
        }
        case 3: { // Hexagon
            for (int i = 0; i < 6; i++) {
                float angle = (i * 2.0f * M_PI / 6.0f) - M_PI / 2.0f;
                points.push_back(cv::Point(
                    cx + (int)(radius * std::cos(angle)),
                    cy + (int)(radius * std::sin(angle))
                ));
            }
            break;
        }
        case 4: { // Star (5-pointed)
            for (int i = 0; i < 10; i++) {
                float angle = (i * 2.0f * M_PI / 10.0f) - M_PI / 2.0f;
                int r = (i % 2 == 0) ? radius : radius / 2;
                points.push_back(cv::Point(
                    cx + (int)(r * std::cos(angle)),
                    cy + (int)(r * std::sin(angle))
                ));
            }
            break;
        }
    }

    return points;
}
