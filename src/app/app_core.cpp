#include "app/app_core.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
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

    // Initialize effect classes
    procedural_shapes_effect_ = std::make_unique<ProceduralShapesEffect>(width_, height_);
    wave_patterns_effect_ = std::make_unique<WavePatternsEffect>(width_, height_);
    mandelbrot_root_veins_effect_ = std::make_unique<MandelbrotRootVeinsEffect>(width_, height_);
}

void AppCore::setSystemMode(SystemMode mode) {
    system_mode_.store(static_cast<int>(mode));
}

SystemMode AppCore::getSystemMode() const {
    return static_cast<SystemMode>(system_mode_.load());
}

void AppCore::setEffect(Effect effect) {
    current_effect_.store(static_cast<int>(effect));
}

Effect AppCore::getEffect() const {
    return static_cast<Effect>(current_effect_.load());
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

    // Get current system mode and effect
    SystemMode current_mode = getSystemMode();
    Effect current_effect = getEffect();

    // Validate effect is allowed in current mode, and adjust if necessary
    bool effect_valid = isEffectValidForMode(current_effect, current_mode);
    if (!effect_valid) {
        // Set to a valid default effect for this mode
        current_effect = getDefaultEffectForMode(current_mode);
        setEffect(current_effect);
    }

    // Process based on current effect
    processEffect(current_effect, in_bgr, out_bgr);
}

bool AppCore::isEffectValidForMode(Effect effect, SystemMode mode) const {
    // All effects are now valid in all modes
    return true;
}

Effect AppCore::getDefaultEffectForMode(SystemMode mode) const {
    switch (mode) {
        case SystemMode::AMBIENT:
            return Effect::PROCEDURAL_SHAPES;  // Default to Procedural Shapes
        case SystemMode::ACTIVE:
            return Effect::FILLED_SILHOUETTE;  // Default to Filled Silhouette
        default:
            return Effect::DEBUG;  // Fallback
    }
}

SystemMode AppCore::getAppropriateModeForEffect(Effect effect) const {
    switch (effect) {
        case Effect::PROCEDURAL_SHAPES:
        case Effect::WAVE_PATTERNS:
        case Effect::MANDELBROT_ROOT_VEINS:
            return SystemMode::AMBIENT;
        case Effect::DEBUG:
            // Debug effect doesn't change mode - keep current mode
            return getSystemMode();
        default:
            return SystemMode::ACTIVE;
    }
}

void AppCore::processEffect(Effect effect, const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    // If in transition, we need to render both old and new effects
    if (transition_frames_remaining_ > 0) {
        // For now, keep the old transition logic but adapt it to effects
        // This would need more work to properly handle effect transitions
        // For simplicity, just render the current effect
        transition_frames_remaining_ = 0;  // Disable transitions for now
    }

    // Process the current effect
    switch (effect) {
        case Effect::DEBUG:
            processPassThrough(in_bgr, out_bgr);
            break;
        case Effect::FILLED_SILHOUETTE:
            processFilledSilhouette(in_bgr, out_bgr);
            break;
        case Effect::OUTLINE_ONLY:
            processOutline(in_bgr, out_bgr);
            break;
        case Effect::MOTION_TRAILS:
            processMotionTrails(in_bgr, out_bgr);
            break;
        case Effect::RAINBOW_MOTION_TRAILS:
            processRainbowTrails(in_bgr, out_bgr);
            break;
        case Effect::DOUBLE_EXPOSURE:
            processDoubleExposure(in_bgr, out_bgr);
            break;
        case Effect::PROCEDURAL_SHAPES:
            if (procedural_shapes_effect_) {
                procedural_shapes_effect_->process(out_bgr, width_, height_);
            }
            break;
        case Effect::WAVE_PATTERNS:
            if (wave_patterns_effect_) {
                wave_patterns_effect_->process(out_bgr, width_, height_);
            }
            break;
        case Effect::MANDELBROT_ROOT_VEINS:
            if (mandelbrot_root_veins_effect_) {
                mandelbrot_root_veins_effect_->process(out_bgr, width_, height_);
            }
            break;
        case Effect::GEOMETRIC_ABSTRACTION:
            processGeometricAbstraction(in_bgr, out_bgr);
            break;
        default:
            processPassThrough(in_bgr, out_bgr);
            break;
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
                    if (procedural_shapes_effect_) {
                        procedural_shapes_effect_->process(out_region);
                    }
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
        // REPEAT mode: Show the same image on each panel with different effects per panel
        // Each panel gets a different effect automatically cycled through available effects
        for (int i = 0; i < num_panels_; i++) {
            int x_start = i * panel_width;
            int x_end = (i == num_panels_ - 1) ? in_bgr.cols : (i + 1) * panel_width;
            int current_panel_width = x_end - x_start;

            // Resize input to single panel size
            cv::Mat resized_input;
            cv::resize(in_bgr, resized_input, cv::Size(current_panel_width, in_bgr.rows));

            // In REPEAT mode, automatically assign different effects to each panel
            // Get all valid effects for current system mode and cycle through them
            std::vector<Effect> valid_effects = getValidEffectsForMode(getSystemMode());
            if (valid_effects.empty()) {
                valid_effects = {Effect::DEBUG};  // fallback
            }

            // Cycle through valid effects based on panel index
            Effect panel_effect = valid_effects[i % valid_effects.size()];

            // Convert effect enum to int for processPanelRegion
            int effect_num = static_cast<int>(panel_effect);

            // Process the resized input with panel-specific effect
            cv::Mat processed_panel;
            processPanelRegion(resized_input, processed_panel, effect_num, i);

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
            // Rainbow trails (renumbered from 6) - use global mode (too complex for per-panel)
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
        case 6:
            // Double exposure (renumbered from 7) - full implementation with per-panel state
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
        case 7:
            // Procedural shapes (renumbered from 8) - generate for this panel region
            {
                if (procedural_shapes_effect_) {
                    procedural_shapes_effect_->process(out_region, w, h);
                }
            }
            break;
        case 8:
            // Wave patterns - generate for this panel region
            {
                if (wave_patterns_effect_) {
                    wave_patterns_effect_->process(out_region, w, h);
                }
            }
            break;
        case 9:
            // Geometric abstraction
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                temp_output = cv::Mat::zeros(h, w, CV_8UC3);
                
                for (const auto& c : contours) {
                    std::vector<cv::Point> approx;
                    double epsilon = 15.0;
                    cv::approxPolyDP(c, approx, epsilon, false);
                    
                    if (approx.size() >= 3) {
                        float area = cv::contourArea(c);
                        float hue = fmod(area * 0.1f, 360.0f);
                        cv::Scalar color = hsvToBgr(hue, 1.0f, 1.0f);
                        cv::fillPoly(temp_output, std::vector<std::vector<cv::Point>>{approx}, color);
                        cv::polylines(temp_output, std::vector<std::vector<cv::Point>>{approx}, true, 
                                     cv::Scalar(255, 255, 255), 2);
                    }
                }
                temp_output.copyTo(out_region);
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
        // In repeat mode, cycle effects on each panel individually
        if (num_panels_ > 1 && getPanelMode() == PanelMode::REPEAT) {
            // Cycle effects on each panel
            for (int panel_index = 0; panel_index < num_panels_; panel_index++) {
                // Get current effect for this panel
                int current_effect_num = panel_effects_[panel_index].load();
                Effect current_effect = static_cast<Effect>(current_effect_num);

                // Get all valid effects for current system mode
                std::vector<Effect> valid_effects = getValidEffectsForMode(getSystemMode());
                if (valid_effects.empty()) {
                    valid_effects = {Effect::DEBUG};
                }

                // Find current effect in the list
                auto it = std::find(valid_effects.begin(), valid_effects.end(), current_effect);
                size_t current_index = (it != valid_effects.end()) ? std::distance(valid_effects.begin(), it) : 0;

                // Cycle to next effect for this panel
                size_t next_index = (current_index + 1) % valid_effects.size();
                Effect next_effect = valid_effects[next_index];

                // Update the panel effect
                panel_effects_[panel_index].store(static_cast<int>(next_effect));
            }

            const char* mode_names[] = {"Ambient", "Active"};
            SystemMode current_mode = getSystemMode();

            std::cout << "[AUTO-CYCLE] [REPEAT MODE] All panels cycled to next effects ("
                      << mode_names[static_cast<int>(current_mode)] << " mode)" << std::endl;
        } else {
            // Normal mode: cycle through effects within current system mode
            SystemMode current_system_mode = getSystemMode();
            Effect current_effect = getEffect();

            // Get list of valid effects for current system mode
            std::vector<Effect> valid_effects = getValidEffectsForMode(current_system_mode);

            // Find current effect in the list
            auto it = std::find(valid_effects.begin(), valid_effects.end(), current_effect);
            size_t current_index = (it != valid_effects.end()) ? std::distance(valid_effects.begin(), it) : 0;

            // Cycle to next effect
            size_t next_index = (current_index + 1) % valid_effects.size();
            Effect next_effect = valid_effects[next_index];

            const char* effect_names[] = {
                "Debug View",
                "Filled Silhouette",
                "Outline Only",
                "Motion Trails",
                "Rainbow Motion Trails",
                "Double Exposure",
                "Procedural Shapes",
                "Wave Patterns",
                "Geometric Abstraction"
            };

            const char* mode_names[] = {
                "Ambient",
                "Active"
            };

            std::cout << "[AUTO-CYCLE] [" << mode_names[static_cast<int>(current_system_mode)] << "] Switching from Effect "
                      << static_cast<int>(current_effect) << " (" << effect_names[static_cast<int>(current_effect) - 1] << ")"
                      << " to Effect " << static_cast<int>(next_effect)
                      << " (" << effect_names[static_cast<int>(next_effect) - 1] << ")" << std::endl;

            setEffect(next_effect);
        }

        // Start transition
        transition_frames_remaining_ = TRANSITION_FRAMES;
        transition_alpha_ = 0.0f;

        // Reset counter and get new random interval
        cycle_frame_counter_ = 0;
        frames_until_next_mode_ = getRandomCycleInterval();
    }
}

std::vector<Effect> AppCore::getValidEffectsForMode(SystemMode mode) const {
    std::vector<Effect> valid_effects;

    switch (mode) {
        case SystemMode::AMBIENT:
            // Ambient mode: Procedural Shapes, Wave Patterns, and Mandelbrot Root Veins
            valid_effects = {Effect::PROCEDURAL_SHAPES, Effect::WAVE_PATTERNS, Effect::MANDELBROT_ROOT_VEINS};
            break;
        case SystemMode::ACTIVE:
            // Active mode: All effects except DEBUG, PROCEDURAL_SHAPES, and WAVE_PATTERNS
            valid_effects = {
                Effect::FILLED_SILHOUETTE,
                Effect::OUTLINE_ONLY,
                Effect::MOTION_TRAILS,
                Effect::RAINBOW_MOTION_TRAILS,
                Effect::DOUBLE_EXPOSURE,
                Effect::GEOMETRIC_ABSTRACTION
            };
            break;
    }

    return valid_effects;
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



// Effect 9: Geometric Abstraction (Active System Mode - Interpretation-based)
void AppCore::processGeometricAbstraction(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    if (in_bgr.empty()) {
        out_bgr = cv::Mat::zeros(height_, width_, CV_8UC3);
        return;
    }
    
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);
    
    // Clean up noise with morphological operations
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(fg_mask, fg_mask, cv::MORPH_OPEN, kernel);  // Remove small noise
    cv::morphologyEx(fg_mask, fg_mask, cv::MORPH_CLOSE, kernel); // Fill small holes
    
    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1000);
    
    out_bgr = cv::Mat::zeros(in_bgr.rows, in_bgr.cols, CV_8UC3);
    
    for (const auto& c : contours) {
        // Approximate contour with fewer points for geometric look
        std::vector<cv::Point> approx;
        double epsilon = 15.0;  // Approximation accuracy
        cv::approxPolyDP(c, approx, epsilon, false);
        
        if (approx.size() >= 3) {
            // Draw simplified polygon with gradient colors
            // Use contour area to generate hue (0-360 range for hsvToBgr)
            float area = cv::contourArea(c);
            float hue = fmod(area * 0.1f, 360.0f);
            cv::Scalar color = hsvToBgr(hue, 1.0f, 1.0f);
            cv::fillPoly(out_bgr, std::vector<std::vector<cv::Point>>{approx}, color);
            
            // Draw outline
            cv::polylines(out_bgr, std::vector<std::vector<cv::Point>>{approx}, true, 
                         cv::Scalar(255, 255, 255), 2);
        }
    }
}
