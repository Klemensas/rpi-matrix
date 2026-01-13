#include "app/app_core.h"

#include <cmath>
#include <cstdlib>
#include <opencv2/imgproc.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AppCore::AppCore(int width, int height, int num_panels)
    : width_(width),
      height_(height),
      num_panels_(num_panels),
      background_subtractor_(cv::createBackgroundSubtractorMOG2(500, 16, true)) {
    silhouette_frame_ = cv::Mat::zeros(height_, width_, CV_8UC3);
    trail_age_buffer_ = cv::Mat::zeros(height_, width_, CV_32FC1);  // Float buffer for age tracking
    
    // Initialize panel effects to default (resources created lazily)
    for (int i = 0; i < num_panels_; i++) {
        panel_effects_[i].store(1);  // Default to pass-through
    }
}

void AppCore::setDisplayMode(int mode) {
    display_mode_.store(mode);
}

int AppCore::displayMode() const {
    return display_mode_.load();
}

void AppCore::setPanelEffect(int panel_index, int effect) {
    if (panel_index >= 0 && panel_index < num_panels_) {
        panel_effects_[panel_index].store(effect);
    }
}

int AppCore::getPanelEffect(int panel_index) const {
    if (panel_index >= 0 && panel_index < num_panels_) {
        return panel_effects_[panel_index].load();
    }
    return 1; // default
}

void AppCore::ensureSize(int w, int h) {
    if (w == width_ && h == height_ && !silhouette_frame_.empty()) return;
    width_ = w;
    height_ = h;
    silhouette_frame_ = cv::Mat::zeros(height_, width_, CV_8UC3);
    trail_age_buffer_ = cv::Mat::zeros(height_, width_, CV_32FC1);
}

void AppCore::setMultiPanelEnabled(bool enabled) {
    multi_panel_enabled_.store(enabled);
}

void AppCore::setPanelMode(PanelMode mode) {
    panel_mode_.store(static_cast<int>(mode));
}

PanelMode AppCore::getPanelMode() const {
    return static_cast<PanelMode>(panel_mode_.load());
}

void AppCore::processFrame(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    if (in_bgr.empty()) return;
    ensureSize(in_bgr.cols, in_bgr.rows);
    
    // Update auto-cycling
    updateAutoCycling();

    // Check if we need special multi-panel processing
    // This happens when:
    // 1. Multi-panel mode is explicitly enabled (different effects per panel), OR
    // 2. We have multiple panels AND panel mode is REPEAT (same view, potentially different effects)
    bool use_multi_panel = multi_panel_enabled_.load() || 
                          (num_panels_ > 1 && getPanelMode() == PanelMode::REPEAT);
    
    if (use_multi_panel) {
        processMultiPanel(in_bgr, out_bgr);
        return;
    }

    // Normal display modes (single panel or extend mode with same effect on all)
    
    // If in transition, we need to render both old and new modes
    if (transition_frames_remaining_ > 0) {
        // Render previous mode to buffer
        cv::Mat prev_output;
        int saved_mode = display_mode_.load();
        display_mode_ = previous_mode_;
        
        switch (previous_mode_) {
            case 1: processPassThrough(in_bgr, prev_output); break;
            case 2: processFilledSilhouette(in_bgr, prev_output); break;
            case 3: processOutline(in_bgr, prev_output); break;
            case 4: processMotionTrails(in_bgr, prev_output); break;
            case 5: processEnergyMotion(in_bgr, prev_output); break;
            case 6: processRainbowTrails(in_bgr, prev_output); break;
            case 7: processDoubleExposure(in_bgr, prev_output); break;
            case 8: processProceduralShapes(prev_output); break;
            default: processPassThrough(in_bgr, prev_output); break;
        }
        
        // Restore mode and render current mode
        display_mode_ = saved_mode;
        cv::Mat curr_output;
        switch (saved_mode) {
            case 1: processPassThrough(in_bgr, curr_output); break;
            case 2: processFilledSilhouette(in_bgr, curr_output); break;
            case 3: processOutline(in_bgr, curr_output); break;
            case 4: processMotionTrails(in_bgr, curr_output); break;
            case 5: processEnergyMotion(in_bgr, curr_output); break;
            case 6: processRainbowTrails(in_bgr, curr_output); break;
            case 7: processDoubleExposure(in_bgr, curr_output); break;
            case 8: processProceduralShapes(curr_output); break;
            default: processPassThrough(in_bgr, curr_output); break;
        }
        
        // Blend based on transition progress
        cv::addWeighted(prev_output, 1.0f - transition_alpha_, 
                       curr_output, transition_alpha_, 0, out_bgr);
    } else {
        // No transition, just render current mode
        switch (display_mode_.load()) {
            case 1: processPassThrough(in_bgr, out_bgr); break;
            case 2: processFilledSilhouette(in_bgr, out_bgr); break;
            case 3: processOutline(in_bgr, out_bgr); break;
            case 4: processMotionTrails(in_bgr, out_bgr); break;
            case 5: processEnergyMotion(in_bgr, out_bgr); break;
            case 6: processRainbowTrails(in_bgr, out_bgr); break;
            case 7: processDoubleExposure(in_bgr, out_bgr); break;
            case 8: processProceduralShapes(out_bgr); break;
            default: processPassThrough(in_bgr, out_bgr); break;
        }
    }
}

void AppCore::processPassThrough(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    out_bgr = in_bgr; // shallow copy ok; display should not mutate
}

static void findPersonContours(const cv::Mat& fg_mask,
                               std::vector<std::vector<cv::Point>>& contours_out,
                               int min_contour_area) {
    std::vector<cv::Vec4i> hierarchy;
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(fg_mask, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    contours_out.clear();
    contours_out.reserve(contours.size());
    for (const auto& c : contours) {
        if (cv::contourArea(c) > min_contour_area) contours_out.push_back(c);
    }
}

void AppCore::processFilledSilhouette(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);

    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1000);

    out_bgr = cv::Mat::zeros(in_bgr.rows, in_bgr.cols, CV_8UC3);
    for (const auto& c : contours) {
        cv::drawContours(out_bgr, std::vector<std::vector<cv::Point>>{c}, -1,
                         cv::Scalar(255, 255, 255), cv::FILLED);
    }
}

void AppCore::processOutline(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);

    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1000);

    out_bgr = cv::Mat::zeros(in_bgr.rows, in_bgr.cols, CV_8UC3);
    for (const auto& c : contours) {
        cv::drawContours(out_bgr, std::vector<std::vector<cv::Point>>{c}, -1,
                         cv::Scalar(255, 255, 255), /*thickness=*/2);
    }
}

void AppCore::processMotionTrails(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);

    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1000);

    silhouette_frame_ *= trail_alpha_;
    for (const auto& c : contours) {
        cv::drawContours(silhouette_frame_, std::vector<std::vector<cv::Point>>{c}, -1,
                         cv::Scalar(255, 255, 255), cv::FILLED);
    }
    out_bgr = silhouette_frame_;
}

void AppCore::processEnergyMotion(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);

    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1000);

    // Fast uniform decay + overwrite with bright new silhouettes
    silhouette_frame_ *= 0.92f;
    for (const auto& c : contours) {
        cv::drawContours(silhouette_frame_, std::vector<std::vector<cv::Point>>{c}, -1,
                         cv::Scalar(255, 255, 255), cv::FILLED);
    }
    out_bgr = silhouette_frame_;
}

void AppCore::processRainbowTrails(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    // Detect current foreground (moving person)
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);
    
    // Apply morphological operations to reduce noise (removes small facial feature detections)
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(fg_mask, fg_mask, cv::MORPH_OPEN, kernel);  // Remove small noise
    cv::morphologyEx(fg_mask, fg_mask, cv::MORPH_CLOSE, kernel); // Fill small holes
    
    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1500);  // Increased from 1000
    
    // Create mask for current foreground
    cv::Mat current_fg_mask = cv::Mat::zeros(in_bgr.rows, in_bgr.cols, CV_8UC1);
    for (const auto& c : contours) {
        cv::drawContours(current_fg_mask, std::vector<std::vector<cv::Point>>{c}, -1,
                        cv::Scalar(255), cv::FILLED);
    }
    
    // Decay existing trails slower for longer-lasting effect
    trail_age_buffer_ *= 0.93f;  // Increased from 0.88 (slower decay = longer trails)
    
    // Add new motion to trail buffer (full brightness)
    trail_age_buffer_.setTo(255.0f, current_fg_mask);
    
    // Convert trail age buffer to 8-bit for processing
    cv::Mat trail_intensity;
    trail_age_buffer_.convertTo(trail_intensity, CV_8UC1);
    
    // Higher threshold to remove more noise
    cv::threshold(trail_intensity, trail_intensity, 20, 255, cv::THRESH_TOZERO);  // Increased from 10
    
    // Create HSV image for rainbow effect
    // Strategy: Use trail intensity for both hue variation AND brightness
    // - Hue: cycles through rainbow (time-based + spatial variation)
    // - Saturation: full (255)
    // - Value: based on trail intensity (brighter = more recent)
    cv::Mat trail_hsv = cv::Mat::zeros(in_bgr.rows, in_bgr.cols, CV_8UC3);
    
    // Create dynamic hue map (time-based cycling for animation effect)
    static float hue_offset = 0.0f;
    hue_offset = fmod(hue_offset + 3.0f, 180.0f);  // Faster animation (was 2.0)
    
    // Vectorized operation: create hue based on intensity + position
    for (int y = 0; y < trail_intensity.rows; y++) {
        uint8_t* intensity_row = trail_intensity.ptr<uint8_t>(y);
        cv::Vec3b* hsv_row = trail_hsv.ptr<cv::Vec3b>(y);
        
        for (int x = 0; x < trail_intensity.cols; x++) {
            uint8_t intensity = intensity_row[x];
            if (intensity > 20) {  // Match new threshold
                // Multi-color rainbow effect:
                // - Base hue from position for spatial variation
                // - Animated with time offset
                // - Intensity affects brightness (V channel)
                float base_hue = fmod((x * 0.5f + y * 0.4f + hue_offset), 180.0f);  // More variation (was 0.3, 0.2)
                uint8_t hue = static_cast<uint8_t>(base_hue);
                uint8_t saturation = 255;  // Full saturation for vivid colors
                
                // Boost brightness for more dramatic effect (gamma correction)
                float normalized = intensity / 255.0f;
                float boosted = std::pow(normalized, 0.7f);  // Gamma < 1 = brighter
                uint8_t value = cv::saturate_cast<uint8_t>(boosted * 255.0f);
                
                hsv_row[x] = cv::Vec3b(hue, saturation, value);
            }
        }
    }
    
    // Convert HSV to BGR (vectorized operation)
    cv::Mat trail_colored;
    cv::cvtColor(trail_hsv, trail_colored, cv::COLOR_HSV2BGR);
    
    // Start with camera feed
    out_bgr = in_bgr.clone();
    
    // Create inverse mask (where trails SHOULD appear - not on current person)
    cv::Mat trail_mask;
    cv::bitwise_not(current_fg_mask, trail_mask);
    
    // Apply alpha blending based on trail intensity (opacity from intensity)
    // Brighter trails = more opaque, fading trails = more transparent
    cv::Mat trail_alpha_float;
    trail_age_buffer_.convertTo(trail_alpha_float, CV_32FC1, 1.0 / 255.0);  // 0.0 to 1.0
    
    // Blend trails onto output using vectorized operation
    for (int y = 0; y < out_bgr.rows; y++) {
        cv::Vec3b* out_row = out_bgr.ptr<cv::Vec3b>(y);
        const cv::Vec3b* trail_row = trail_colored.ptr<cv::Vec3b>(y);
        const cv::Vec3b* cam_row = in_bgr.ptr<cv::Vec3b>(y);
        const uint8_t* mask_row = trail_mask.ptr<uint8_t>(y);
        const float* alpha_row = trail_alpha_float.ptr<float>(y);
        
        for (int x = 0; x < out_bgr.cols; x++) {
            if (mask_row[x] > 0) {  // Only where no current person
                float alpha = alpha_row[x];
                if (alpha > 0.08f) {  // Higher threshold to match trail threshold (was 0.04)
                    // Alpha blend: out = trail * alpha + camera * (1 - alpha)
                    // Boost alpha for more dramatic visibility
                    float boosted_alpha = std::min(1.0f, alpha * 1.2f);
                    out_row[x][0] = cv::saturate_cast<uint8_t>(trail_row[x][0] * boosted_alpha + cam_row[x][0] * (1.0f - boosted_alpha));
                    out_row[x][1] = cv::saturate_cast<uint8_t>(trail_row[x][1] * boosted_alpha + cam_row[x][1] * (1.0f - boosted_alpha));
                    out_row[x][2] = cv::saturate_cast<uint8_t>(trail_row[x][2] * boosted_alpha + cam_row[x][2] * (1.0f - boosted_alpha));
                }
            }
        }
    }
}

void AppCore::processDoubleExposure(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    processDoubleExposureWithState(in_bgr, out_bgr, 
                                   frame_history_, 
                                   frame_history_index_, 
                                   frame_counter_, 
                                   current_time_offset_,
                                   background_subtractor_);
}

void AppCore::processProceduralShapes(cv::Mat& out_bgr) {
    // Initialize output with black background
    out_bgr = cv::Mat::zeros(height_, width_, CV_8UC3);
    
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
cv::Scalar AppCore::hsvToBgr(float h, float s, float v) {
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
void AppCore::drawMorphingShape(cv::Mat& img, int cx, int cy, int radius, 
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
std::vector<cv::Point> AppCore::getShapePoints(int shape_type, int cx, int cy, int radius) {
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

void AppCore::processDoubleExposureWithState(const cv::Mat& in_bgr, cv::Mat& out_bgr,
                                             std::vector<cv::Mat>& history,
                                             int& history_index,
                                             int& frame_counter,
                                             int& time_offset,
                                             cv::Ptr<cv::BackgroundSubtractor> bg_subtractor) {
    // Ensure frame history buffer is initialized
    if (history.empty()) {
        history.resize(MAX_FRAME_HISTORY);
    }
    
    // Store current frame in history
    in_bgr.copyTo(history[history_index]);
    history_index = (history_index + 1) % MAX_FRAME_HISTORY;
    
    // Change time offset randomly every 60 frames (~2 seconds at 30fps)
    frame_counter++;
    if (frame_counter >= 60) {
        // Generate random offset between MIN_TIME_OFFSET and MAX_TIME_OFFSET
        time_offset = MIN_TIME_OFFSET + (rand() % (MAX_TIME_OFFSET - MIN_TIME_OFFSET + 1));
        frame_counter = 0;
    }
    
    // Calculate index for past frame with current random offset
    int past_frame_index = (history_index - time_offset + MAX_FRAME_HISTORY) % MAX_FRAME_HISTORY;
    
    // Check if past frame exists
    if (!history[past_frame_index].empty()) {
        // Detect motion using background subtractor
        cv::Mat fg_mask;
        bg_subtractor->apply(in_bgr, fg_mask);
        
        // Minimal cleanup
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
        cv::morphologyEx(fg_mask, fg_mask, cv::MORPH_CLOSE, kernel);
        
        // Blur for smooth edges
        cv::GaussianBlur(fg_mask, fg_mask, cv::Size(15, 15), 0);
        
        // Create stronger double exposure blend (25% current, 75% past for more opaque ghosting)
        cv::Mat blended;
        cv::addWeighted(in_bgr, 0.25, history[past_frame_index], 0.75, 0, blended);
        
        // Fast blending without float conversion:
        // Use OpenCV's built-in blending with mask weights
        out_bgr = in_bgr.clone();
        
        // Apply stronger double exposure where mask is above threshold
        // This avoids all float conversions and uses optimized copyTo
        blended.copyTo(out_bgr, fg_mask);
    } else {
        // Not enough history yet, just pass through
        out_bgr = in_bgr.clone();
    }
}

void AppCore::ensurePanelResourcesInitialized() {
    if (!panel_resources_initialized_) {
        panel_bg_subtractors_.resize(num_panels_);
        panel_silhouette_frames_.resize(num_panels_);
        
        // Initialize per-panel Mode 7 (Double Exposure) resources
        panel_frame_history_.resize(num_panels_);
        panel_frame_history_index_.resize(num_panels_, 0);
        panel_frame_counter_.resize(num_panels_, 0);
        panel_time_offset_.resize(num_panels_, MIN_TIME_OFFSET);
        
        for (int i = 0; i < num_panels_; i++) {
            panel_bg_subtractors_[i] = cv::createBackgroundSubtractorMOG2(500, 16, true);
            panel_silhouette_frames_[i] = cv::Mat::zeros(height_, width_ / num_panels_, CV_8UC3);
            // Frame history will be lazy-initialized when Mode 7 is first used
        }
        panel_resources_initialized_ = true;
    }
}

void AppCore::processMultiPanel(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    // Lazy initialization of per-panel resources
    ensurePanelResourcesInitialized();
    
    PanelMode mode = getPanelMode();
    int panel_width = in_bgr.cols / num_panels_;
    bool individual_effects = multi_panel_enabled_.load();
    
    out_bgr = cv::Mat(in_bgr.rows, in_bgr.cols, CV_8UC3);
    
    // Special handling for Mode 7 (Double Exposure) in EXTEND mode when NOT using individual effects
    // In REPEAT mode or when using individual effects, each panel gets its own state via processPanelRegion
    bool use_fullframe_mode7 = false;
    if (mode == PanelMode::EXTEND && !individual_effects && display_mode_.load() == 7) {
        use_fullframe_mode7 = true;
    }
    
    if (mode == PanelMode::EXTEND) {
        // EXTEND mode: Split input horizontally across panels
        
        // If Mode 7 is used in shared mode, process full frame first, then split
        if (use_fullframe_mode7) {
            cv::Mat processed_full;
            processDoubleExposure(in_bgr, processed_full);
            
            // Now split across panels
            for (int i = 0; i < num_panels_; i++) {
                int x_start = i * panel_width;
                int x_end = (i == num_panels_ - 1) ? in_bgr.cols : (i + 1) * panel_width;
                
                int effect = individual_effects ? panel_effects_[i].load() : display_mode_.load();
                
                cv::Rect panel_roi(x_start, 0, x_end - x_start, in_bgr.rows);
                cv::Mat out_region = out_bgr(panel_roi);
                
                if (effect == 7) {
                    // Use the processed full frame
                    processed_full(panel_roi).copyTo(out_region);
                } else if (effect == 8) {
                    // Procedural shapes - generate for this panel region
                    int saved_w = width_;
                    int saved_h = height_;
                    width_ = out_region.cols;
                    height_ = out_region.rows;
                    processProceduralShapes(out_region);
                    width_ = saved_w;
                    height_ = saved_h;
                } else {
                    // Use standard processing for other effects
                    cv::Mat in_region = in_bgr(panel_roi);
                    processPanelRegion(in_region, out_region, effect, i);
                }
            }
        } else {
            // Standard extend mode (no Mode 7)
            for (int i = 0; i < num_panels_; i++) {
                int x_start = i * panel_width;
                int x_end = (i == num_panels_ - 1) ? in_bgr.cols : (i + 1) * panel_width;
                
                cv::Rect panel_roi(x_start, 0, x_end - x_start, in_bgr.rows);
                cv::Mat in_region = in_bgr(panel_roi);
                cv::Mat out_region = out_bgr(panel_roi);
                
                int effect = individual_effects ? panel_effects_[i].load() : display_mode_.load();
                processPanelRegion(in_region, out_region, effect, i);
            }
        }
    } else {
        // REPEAT mode: Show the same image on each panel with independent per-panel effects
        // Each panel always gets its own state (including independent Mode 7 ghosting)
        for (int i = 0; i < num_panels_; i++) {
            int x_start = i * panel_width;
            int x_end = (i == num_panels_ - 1) ? in_bgr.cols : (i + 1) * panel_width;
            int current_panel_width = x_end - x_start;
            
            // Resize input to single panel size
            cv::Mat resized_input;
            cv::resize(in_bgr, resized_input, cv::Size(current_panel_width, in_bgr.rows));
            
            // Use panel-specific effect if multi_panel_enabled, otherwise use global display_mode
            int effect = individual_effects ? panel_effects_[i].load() : display_mode_.load();
            
            // Process the resized input with panel-specific effect
            // (Mode 7 will use per-panel state via processPanelRegion for individual ghosting)
            cv::Mat processed_panel;
            processPanelRegion(resized_input, processed_panel, effect, i);
            
            // Copy processed panel to output region
            cv::Rect panel_roi(x_start, 0, current_panel_width, in_bgr.rows);
            cv::Mat out_region = out_bgr(panel_roi);
            processed_panel.copyTo(out_region);
        }
    }
}

void AppCore::processPanelRegion(const cv::Mat& in_region, cv::Mat& out_region, int effect, int panel_index) {
    // Ensure per-panel buffers are correctly sized
    int w = in_region.cols;
    int h = in_region.rows;
    if (panel_silhouette_frames_[panel_index].cols != w || 
        panel_silhouette_frames_[panel_index].rows != h) {
        panel_silhouette_frames_[panel_index] = cv::Mat::zeros(h, w, CV_8UC3);
    }
    
    cv::Mat temp_output;
    
    // Apply the specified effect to this region
    switch (effect) {
        case 1:
            // Pass-through
            in_region.copyTo(out_region);
            break;
        case 2:
            // Filled silhouette
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                temp_output = cv::Mat::zeros(h, w, CV_8UC3);
                for (const auto& c : contours) {
                    cv::drawContours(temp_output, std::vector<std::vector<cv::Point>>{c}, -1,
                                   cv::Scalar(255, 255, 255), cv::FILLED);
                }
                temp_output.copyTo(out_region);
            }
            break;
        case 3:
            // Outline
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                temp_output = cv::Mat::zeros(h, w, CV_8UC3);
                for (const auto& c : contours) {
                    cv::drawContours(temp_output, std::vector<std::vector<cv::Point>>{c}, -1,
                                   cv::Scalar(255, 255, 255), /*thickness=*/2);
                }
                temp_output.copyTo(out_region);
            }
            break;
        case 4:
            // Motion trails
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                panel_silhouette_frames_[panel_index] *= 0.7f;
                for (const auto& c : contours) {
                    cv::drawContours(panel_silhouette_frames_[panel_index], 
                                   std::vector<std::vector<cv::Point>>{c}, -1,
                                   cv::Scalar(255, 255, 255), cv::FILLED);
                }
                panel_silhouette_frames_[panel_index].copyTo(out_region);
            }
            break;
        case 5:
            // Energy motion
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                panel_silhouette_frames_[panel_index] *= 0.92f;
                for (const auto& c : contours) {
                    cv::drawContours(panel_silhouette_frames_[panel_index], 
                                   std::vector<std::vector<cv::Point>>{c}, -1,
                                   cv::Scalar(255, 255, 255), cv::FILLED);
                }
                panel_silhouette_frames_[panel_index].copyTo(out_region);
            }
            break;
        case 6:
            // Rainbow trails - use global mode (too complex for per-panel)
            // Just apply a simple version: camera feed + rainbow motion overlay
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                // Simple rainbow effect: color code the current motion
                temp_output = in_region.clone();
                for (size_t i = 0; i < contours.size(); i++) {
                    // Use contour index for color variation
                    float hue = fmod(i * 60.0f + (panel_index * 30.0f), 180.0f);
                    cv::Mat hsv_color(1, 1, CV_8UC3, cv::Scalar(hue, 255, 255));
                    cv::Mat bgr_color;
                    cv::cvtColor(hsv_color, bgr_color, cv::COLOR_HSV2BGR);
                    cv::Scalar color(bgr_color.at<cv::Vec3b>(0, 0)[0],
                                   bgr_color.at<cv::Vec3b>(0, 0)[1],
                                   bgr_color.at<cv::Vec3b>(0, 0)[2]);
                    
                    cv::drawContours(temp_output, contours, i, color, 3);
                }
                temp_output.copyTo(out_region);
            }
            break;
        case 7:
            // Double exposure - full implementation with per-panel state
            {
                cv::Mat temp_output;
                processDoubleExposureWithState(in_region, temp_output,
                                               panel_frame_history_[panel_index],
                                               panel_frame_history_index_[panel_index],
                                               panel_frame_counter_[panel_index],
                                               panel_time_offset_[panel_index],
                                               panel_bg_subtractors_[panel_index]);
                temp_output.copyTo(out_region);
            }
            break;
        case 8:
            // Procedural shapes - generate for this panel region
            {
                // Temporarily set size to panel region size
                int saved_w = width_;
                int saved_h = height_;
                width_ = w;
                height_ = h;
                
                processProceduralShapes(out_region);
                
                // Restore size
                width_ = saved_w;
                height_ = saved_h;
            }
            break;
        default:
            in_region.copyTo(out_region);
            break;
    }
}

void AppCore::updateAutoCycling() {
    if (!auto_cycling_enabled_) {
        return;
    }
    
    cycle_frame_counter_++;
    
    // Update transition if in progress
    if (transition_frames_remaining_ > 0) {
        transition_frames_remaining_--;
        // Calculate alpha: 0.0 (start) -> 1.0 (end)
        transition_alpha_ = 1.0f - ((float)transition_frames_remaining_ / TRANSITION_FRAMES);
        return;
    }
    
    // Initialize on first run
    if (frames_until_next_mode_ == 0) {
        frames_until_next_mode_ = getRandomCycleInterval();
    }
    
    // Check if it's time to cycle
    if (cycle_frame_counter_ >= frames_until_next_mode_) {
        // Save current mode as previous
        previous_mode_ = display_mode_.load();
        
        // Cycle to next mode (1->2->3->4->5->6->7->8->1)
        int next_mode = (display_mode_.load() % 8) + 1;
        display_mode_ = next_mode;
        
        // Start transition
        transition_frames_remaining_ = TRANSITION_FRAMES;
        transition_alpha_ = 0.0f;
        
        // Reset counter and get new random interval
        cycle_frame_counter_ = 0;
        frames_until_next_mode_ = getRandomCycleInterval();
    }
}

int AppCore::getRandomCycleInterval() {
    // Generate random seconds between MIN and MAX
    int seconds = MIN_CYCLE_SECONDS + (rand() % (MAX_CYCLE_SECONDS - MIN_CYCLE_SECONDS + 1));
    // Convert to frames (assuming 30 fps)
    return seconds * 30;
}

void AppCore::toggleAutoCycling() {
    auto_cycling_enabled_ = !auto_cycling_enabled_;
    if (auto_cycling_enabled_) {
        // Reset counters when re-enabling
        cycle_frame_counter_ = 0;
        frames_until_next_mode_ = getRandomCycleInterval();
        transition_frames_remaining_ = 0;
    }
}
