#include "effects/ambient/mandelbrot_root_veins.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <opencv2/imgproc.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MandelbrotRootVeinsEffect::MandelbrotRootVeinsEffect(int width, int height)
    : width_(width), height_(height) {
    reset();
}

void MandelbrotRootVeinsEffect::reset() {
    segments_.clear();
    time_ = 0.0f;
    zoom_ = 1.0f;
    rotation_ = 0.0f;
    initializeRootVeins();
}

void MandelbrotRootVeinsEffect::initializeRootVeins() {
    // Create root veins from each corner toward the center
    float cx = width_ / 2.0f;
    float cy = height_ / 2.0f;
    
    struct CornerInfo {
        cv::Point2f pos;
        float angle;
    };
    
    CornerInfo corners[4] = {
        {{0.0f, 0.0f}, static_cast<float>(atan2(cy, cx))},                           // Top-left
        {{static_cast<float>(width_), 0.0f}, static_cast<float>(atan2(cy, -cx))},    // Top-right
        {{0.0f, static_cast<float>(height_)}, static_cast<float>(atan2(-cy, cx))},   // Bottom-left
        {{static_cast<float>(width_), static_cast<float>(height_)}, static_cast<float>(atan2(-cy, -cx))} // Bottom-right
    };
    
    for (int i = 0; i < 4; i++) {
        VeinSegment root;
        root.start = corners[i].pos;
        root.end = corners[i].pos;  // Will grow from here
        root.age = 0.0f;
        root.generation = 0;
        root.is_wilting = false;
        root.wilt_progress = 0.0f;
        root.phase = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
        root.direction = corners[i].angle;
        root.is_tip = true;
        segments_.push_back(root);
    }
}

float MandelbrotRootVeinsEffect::getMandelbrotDirection(float x, float y, float base_angle) {
    // Normalize coordinates to [-2, 2] range (Mandelbrot space)
    float mx = (x / width_) * 4.0f - 2.0f;
    float my = (y / height_) * 4.0f - 2.0f;
    
    // Perform a few Mandelbrot iterations to get organic curvature
    float zx = 0.0f, zy = 0.0f;
    float cx = mx, cy = my;
    
    int max_iter = 8;
    for (int i = 0; i < max_iter; i++) {
        float new_zx = zx * zx - zy * zy + cx;
        float new_zy = 2.0f * zx * zy + cy;
        zx = new_zx;
        zy = new_zy;
        
        if (zx * zx + zy * zy > 4.0f) break;
    }
    
    // Use the escape direction to influence the growth angle
    float escape_angle = atan2(zy, zx);
    
    // Blend base direction with Mandelbrot influence
    float influence = 0.3f;  // How much Mandelbrot affects direction
    return base_angle + influence * sin(escape_angle + time_ * 0.1f);
}

void MandelbrotRootVeinsEffect::growVeins(float dt) {
    // Find and grow tip segments
    for (auto& seg : segments_) {
        if (!seg.is_tip || seg.is_wilting) continue;
        
        seg.age += dt;
        
        // Calculate growth direction with Mandelbrot influence
        float dir = getMandelbrotDirection(seg.end.x, seg.end.y, seg.direction);
        
        // Add slight time-based variation for organic movement
        dir += 0.1f * sin(time_ * 0.5f + seg.phase);
        
        // Grow the tip
        float growth = GROWTH_SPEED * dt * 30.0f;  // Scale for ~30fps
        cv::Point2f new_end;
        new_end.x = seg.end.x + growth * cos(dir);
        new_end.y = seg.end.y + growth * sin(dir);
        
        // Check bounds - if we've gone too far, stop this tip
        float margin = 20.0f;
        if (new_end.x < -margin || new_end.x > width_ + margin ||
            new_end.y < -margin || new_end.y > height_ + margin) {
            seg.is_tip = false;
            continue;
        }
        
        // If segment has grown long enough, split into new segment
        float seg_len = cv::norm(new_end - seg.start);
        if (seg_len > 15.0f) {
            // Create new tip segment continuing from here
            VeinSegment new_seg;
            new_seg.start = seg.end;
            new_seg.end = new_end;
            new_seg.age = 0.0f;
            new_seg.generation = seg.generation;
            new_seg.is_wilting = false;
            new_seg.wilt_progress = 0.0f;
            new_seg.phase = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
            new_seg.direction = dir;
            new_seg.is_tip = true;
            
            // Old segment is no longer a tip
            seg.is_tip = false;
            
            if (segments_.size() < MAX_SEGMENTS) {
                segments_.push_back(new_seg);
            }
        } else {
            seg.end = new_end;
            seg.direction = dir;
        }
    }
}

bool MandelbrotRootVeinsEffect::segmentsIntersect(
    const cv::Point2f& p1, const cv::Point2f& p2,
    const cv::Point2f& p3, const cv::Point2f& p4,
    cv::Point2f& intersection) {
    
    float d1x = p2.x - p1.x;
    float d1y = p2.y - p1.y;
    float d2x = p4.x - p3.x;
    float d2y = p4.y - p3.y;
    
    float cross = d1x * d2y - d1y * d2x;
    if (std::abs(cross) < 1e-6f) return false;  // Parallel
    
    float dx = p3.x - p1.x;
    float dy = p3.y - p1.y;
    
    float t = (dx * d2y - dy * d2x) / cross;
    float u = (dx * d1y - dy * d1x) / cross;
    
    // Check if intersection is within both segments
    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
        intersection.x = p1.x + t * d1x;
        intersection.y = p1.y + t * d1y;
        return true;
    }
    return false;
}

void MandelbrotRootVeinsEffect::checkIntersections() {
    if (segments_.size() < 2) return;
    
    std::vector<std::pair<cv::Point2f, float>> new_branches;
    
    // Check for intersections between segments
    for (size_t i = 0; i < segments_.size(); i++) {
        if (segments_[i].is_wilting) continue;
        
        for (size_t j = i + 2; j < segments_.size(); j++) {  // Skip adjacent
            if (segments_[j].is_wilting) continue;
            
            cv::Point2f intersection;
            if (segmentsIntersect(segments_[i].start, segments_[i].end,
                                  segments_[j].start, segments_[j].end,
                                  intersection)) {
                // Average direction with random offset
                float avg_dir = (segments_[i].direction + segments_[j].direction) / 2.0f;
                avg_dir += (static_cast<float>(rand()) / RAND_MAX - 0.5f) * BRANCH_ANGLE_SPREAD;
                
                int max_gen = std::max(segments_[i].generation, segments_[j].generation);
                if (max_gen < MAX_GENERATION) {
                    new_branches.push_back({intersection, avg_dir});
                }
            }
        }
    }
    
    // Spawn new branches from intersections
    for (const auto& branch : new_branches) {
        if (segments_.size() >= MAX_SEGMENTS) {
            // Beyond cap - mark oldest non-root segments for wilting
            for (auto& seg : segments_) {
                if (seg.generation > 0 && !seg.is_wilting && seg.age > 2.0f) {
                    seg.is_wilting = true;
                    break;
                }
            }
        } else {
            // Find parent generation from nearby segments
            int parent_gen = 0;
            for (const auto& seg : segments_) {
                float dist = cv::norm(seg.end - branch.first);
                if (dist < 20.0f) {
                    parent_gen = std::max(parent_gen, seg.generation);
                }
            }
            spawnBranch(branch.first, branch.second, parent_gen);
        }
    }
}

void MandelbrotRootVeinsEffect::spawnBranch(const cv::Point2f& origin, float base_direction, int parent_generation) {
    if (segments_.size() >= MAX_SEGMENTS) return;
    
    VeinSegment branch;
    branch.start = origin;
    branch.end = origin;
    branch.age = 0.0f;
    branch.generation = parent_generation + 1;
    branch.is_wilting = false;
    branch.wilt_progress = 0.0f;
    branch.phase = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
    branch.direction = base_direction + (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.5f;
    branch.is_tip = true;
    
    segments_.push_back(branch);
}

void MandelbrotRootVeinsEffect::updateWilting(float dt) {
    for (auto& seg : segments_) {
        if (seg.is_wilting) {
            seg.wilt_progress += WILT_SPEED;
            if (seg.wilt_progress >= 1.0f) {
                seg.wilt_progress = 1.0f;
            }
        }
    }

    // Remove fully wilted segments periodically (but safely)
    if (static_cast<int>(time_ * 10) % 50 == 0) {
        auto it = std::remove_if(segments_.begin(), segments_.end(),
            [](const VeinSegment& s) { return s.wilt_progress >= 1.0f && s.generation > 0; });
        segments_.erase(it, segments_.end());
    }
}

cv::Point2f MandelbrotRootVeinsEffect::applyZoomRotation(const cv::Point2f& p) {
    float cx = width_ / 2.0f;
    float cy = height_ / 2.0f;
    
    // Translate to center
    float dx = p.x - cx;
    float dy = p.y - cy;
    
    // Apply rotation
    float cos_r = cos(rotation_);
    float sin_r = sin(rotation_);
    float rx = dx * cos_r - dy * sin_r;
    float ry = dx * sin_r + dy * cos_r;
    
    // Apply zoom (inverse - zoom out over time to show more network)
    float inv_zoom = 1.0f / zoom_;
    rx *= inv_zoom;
    ry *= inv_zoom;
    
    // Translate back
    return cv::Point2f(rx + cx, ry + cy);
}

float MandelbrotRootVeinsEffect::getSegmentBrightness(const VeinSegment& seg) {
    // Base brightness with low-frequency pulsation
    float pulse = 0.7f + 0.3f * sin(time_ * 0.5f + seg.phase);
    
    // Fade based on generation (deeper branches are slightly dimmer)
    float gen_fade = 1.0f - seg.generation * 0.1f;
    gen_fade = std::max(0.4f, gen_fade);
    
    // Apply wilting fade
    float wilt_fade = 1.0f - seg.wilt_progress;
    
    return pulse * gen_fade * wilt_fade;
}

void MandelbrotRootVeinsEffect::renderVeins(cv::Mat& frame) {
    frame = cv::Mat::zeros(frame.rows, frame.cols, CV_8UC3);
    
    // Draw all segments
    for (const auto& seg : segments_) {
        if (seg.wilt_progress >= 1.0f) continue;
        
        // Apply zoom/rotation transform
        cv::Point2f start = applyZoomRotation(seg.start);
        cv::Point2f end = applyZoomRotation(seg.end);
        
        // Scale to processing resolution
        float scale_x = static_cast<float>(frame.cols) / width_;
        float scale_y = static_cast<float>(frame.rows) / height_;
        
        cv::Point p1(static_cast<int>(start.x * scale_x), static_cast<int>(start.y * scale_y));
        cv::Point p2(static_cast<int>(end.x * scale_x), static_cast<int>(end.y * scale_y));
        
        float brightness = getSegmentBrightness(seg);
        
        // Color: soft cyan/teal with slight hue variation based on generation
        float hue_shift = seg.generation * 15.0f;
        int b = static_cast<int>(200 * brightness);
        int g = static_cast<int>((180 - hue_shift) * brightness);
        int r = static_cast<int>((80 + hue_shift * 0.5f) * brightness);
        
        cv::Scalar color(b, g, r);
        
        // Line thickness based on generation (roots thicker)
        int thickness = std::max(1, 3 - seg.generation);
        
        cv::line(frame, p1, p2, color, thickness, cv::LINE_AA);
    }
}

void MandelbrotRootVeinsEffect::process(cv::Mat& out_bgr, int target_width, int target_height) {
    // Safety check for invalid dimensions
    if (width_ <= 0 || height_ <= 0) {
        out_bgr = cv::Mat::zeros(480, 640, CV_8UC3);
        return;
    }

    int output_width = (target_width > 0) ? target_width : width_;
    int output_height = (target_height > 0) ? target_height : height_;

    // More safety checks
    if (output_width <= 0 || output_height <= 0) {
        output_width = 640;
        output_height = 480;
    }

    float dt = 1.0f / 30.0f;  // Assume ~30fps
    time_ += dt;

    // Update zoom and rotation with safety checks
    if (zoom_ > 0.0f) {
        zoom_ *= (1.0f + ZOOM_RATE);
    } else {
        zoom_ = 1.0f;
    }
    rotation_ += ROTATION_RATE;

    // Reset zoom periodically to prevent infinite zoom
    if (zoom_ > 2.0f || zoom_ <= 0.0f) {
        zoom_ = 1.0f;
        rotation_ = 0.0f;
        // Optionally reinitialize for fresh growth
        if (segments_.size() > MAX_SEGMENTS / 2) {
            reset();
        }
    }

    // Grow veins
    growVeins(dt);

    // Check intersections periodically (expensive)
    if (static_cast<int>(time_ * 30) % 5 == 0) {
        checkIntersections();
    }

    // Update wilting
    updateWilting(dt);

    // Process at half resolution for performance
    int proc_w = output_width / 2;
    int proc_h = output_height / 2;
    if (proc_w < 1) proc_w = 1;
    if (proc_h < 1) proc_h = 1;

    try {
        proc_frame_ = cv::Mat::zeros(proc_h, proc_w, CV_8UC3);

        // Render veins
        renderVeins(proc_frame_);

        // Apply glow effect via blur and additive blend
        cv::GaussianBlur(proc_frame_, glow_frame_, cv::Size(7, 7), 2.0);
        cv::addWeighted(proc_frame_, 1.0, glow_frame_, 0.6, 0, proc_frame_);

        // Upscale to output resolution
        cv::resize(proc_frame_, out_bgr, cv::Size(output_width, output_height), 0, 0, cv::INTER_LINEAR);
    } catch (const cv::Exception& e) {
        // If OpenCV operations fail, return a safe fallback
        std::cerr << "OpenCV error in mandelbrot effect: " << e.what() << std::endl;
        out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);
    } catch (...) {
        // Catch any other exceptions
        std::cerr << "Unknown error in mandelbrot effect" << std::endl;
        out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);
    }
}
