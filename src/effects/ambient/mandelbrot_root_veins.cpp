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
    // Seed random for variety
    srand(static_cast<unsigned>(time(nullptr)));
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
    segments_.clear();
    
    // Work in normalized coordinates [0, 1] to avoid dimension issues
    // We'll scale to actual output size during rendering
    
    // Create root veins from each corner, pointing toward center (0.5, 0.5)
    struct CornerStart {
        float x, y;
        float angle;  // Angle toward center
    };
    
    // All 4 corners with angles pointing toward center
    CornerStart corners[4] = {
        {0.0f, 0.0f, static_cast<float>(atan2(0.5f, 0.5f))},           // Top-left
        {1.0f, 0.0f, static_cast<float>(atan2(0.5f, -0.5f))},          // Top-right  
        {0.0f, 1.0f, static_cast<float>(atan2(-0.5f, 0.5f))},          // Bottom-left
        {1.0f, 1.0f, static_cast<float>(atan2(-0.5f, -0.5f))}          // Bottom-right
    };
    
    // Create 3 veins per corner for denser network
    for (int corner = 0; corner < 4; corner++) {
        for (int vein = 0; vein < 3; vein++) {
            VeinSegment root;
            // Start slightly inward from corner
            float offset = 0.02f;
            root.start.x = corners[corner].x + (corners[corner].x < 0.5f ? offset : -offset);
            root.start.y = corners[corner].y + (corners[corner].y < 0.5f ? offset : -offset);
            root.end = root.start;
            root.age = 0.0f;
            root.generation = 0;
            root.is_wilting = false;
            root.wilt_progress = 0.0f;
            root.phase = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
            // Spread veins slightly around the center direction
            float spread = (vein - 1) * 0.25f;  // -0.25, 0, +0.25 radians
            root.direction = corners[corner].angle + spread;
            root.is_tip = true;
            segments_.push_back(root);
        }
    }
}

float MandelbrotRootVeinsEffect::getMandelbrotDirection(float x, float y, float base_angle) {
    // Map normalized coords to Mandelbrot space [-2, 2]
    float mx = x * 4.0f - 2.0f;
    float my = y * 4.0f - 2.0f;
    
    // Perform Mandelbrot iterations
    float zx = 0.0f, zy = 0.0f;
    
    int max_iter = 10;
    for (int i = 0; i < max_iter; i++) {
        float new_zx = zx * zx - zy * zy + mx;
        float new_zy = 2.0f * zx * zy + my;
        zx = new_zx;
        zy = new_zy;
        
        if (zx * zx + zy * zy > 4.0f) break;
    }
    
    // Use escape trajectory for organic curves
    float escape_angle = atan2(zy, zx);
    
    // Subtle influence - just add gentle curves
    return base_angle + 0.15f * sin(escape_angle + time_ * 0.5f);
}

void MandelbrotRootVeinsEffect::growVeins(float dt) {
    const float center_x = 0.5f;
    const float center_y = 0.5f;
    
    std::vector<VeinSegment> new_segments;
    
    for (auto& seg : segments_) {
        if (!seg.is_tip || seg.is_wilting) continue;
        
        seg.age += dt;
        
        // Re-calculate direction toward center with some persistence
        float to_center_angle = atan2(center_y - seg.end.y, center_x - seg.end.x);
        
        // Blend current direction with center direction
        float blend = 0.85f;
        float base_dir = seg.direction * blend + to_center_angle * (1.0f - blend);
        
        // Add Mandelbrot-influenced curvature
        float dir = getMandelbrotDirection(seg.end.x, seg.end.y, base_dir);
        
        // Add organic wiggle - more pronounced for realistic veins
        dir += 0.15f * sin(time_ * 2.0f + seg.phase * 3.0f + seg.end.x * 15.0f);
        dir += 0.08f * cos(time_ * 1.2f + seg.phase * 2.0f + seg.end.y * 12.0f);
        
        // Slower growth for finer detail
        float speed_factor = 1.0f / (1.0f + seg.generation * 0.25f);
        float growth = 0.005f * speed_factor;
        
        cv::Point2f new_end;
        new_end.x = seg.end.x + growth * cos(dir);
        new_end.y = seg.end.y + growth * sin(dir);
        
        // Check if reached center area or went out of bounds
        float dist_to_center = sqrt((new_end.x - center_x) * (new_end.x - center_x) + 
                                    (new_end.y - center_y) * (new_end.y - center_y));
        
        if (dist_to_center < 0.06f ||
            new_end.x < -0.02f || new_end.x > 1.02f ||
            new_end.y < -0.02f || new_end.y > 1.02f) {
            seg.is_tip = false;
            continue;
        }
        
        // Update segment endpoint
        seg.end = new_end;
        seg.direction = dir;
        
        // Calculate segment length
        float seg_length = sqrt((seg.end.x - seg.start.x) * (seg.end.x - seg.start.x) +
                                (seg.end.y - seg.start.y) * (seg.end.y - seg.start.y));
        
        // Much more frequent branching for vein-like appearance
        // Shorter segments before branching, especially for early generations
        float branch_length = 0.02f + seg.generation * 0.008f;
        
        if (seg_length > branch_length && segments_.size() + new_segments.size() < MAX_SEGMENTS) {
            
            // High branching probability - veins branch a lot
            float branch_prob = 0.7f - seg.generation * 0.08f;
            branch_prob = std::max(0.15f, branch_prob);
            
            bool should_branch = (static_cast<float>(rand()) / RAND_MAX < branch_prob) && 
                                 (seg.generation < MAX_GENERATION);
            
            if (should_branch) {
                // Create TWO branches for Y-shaped splits (more vein-like)
                for (int b = 0; b < 2; b++) {
                    VeinSegment branch;
                    branch.start = seg.end;
                    branch.end = seg.end;
                    branch.age = 0.0f;
                    branch.generation = seg.generation + 1;
                    branch.is_wilting = false;
                    branch.wilt_progress = 0.0f;
                    branch.phase = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
                    
                    // Asymmetric branching angles for natural look
                    float angle_variance = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.3f;
                    float branch_angle = (b == 0) ? 
                        (BRANCH_ANGLE_SPREAD * 0.7f + angle_variance) : 
                        (-BRANCH_ANGLE_SPREAD * 0.7f + angle_variance);
                    branch.direction = dir + branch_angle;
                    branch.is_tip = true;
                    
                    new_segments.push_back(branch);
                }
                seg.is_tip = false;  // Original segment stops
            } else {
                // No branch - just continue with a new segment (for smoother curves)
                VeinSegment continuation;
                continuation.start = seg.end;
                continuation.end = seg.end;
                continuation.age = 0.0f;
                continuation.generation = seg.generation;
                continuation.is_wilting = false;
                continuation.wilt_progress = 0.0f;
                continuation.phase = seg.phase + 0.05f;
                // Add slight direction change for organic curves
                continuation.direction = dir + (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.2f;
                continuation.is_tip = true;
                
                new_segments.push_back(continuation);
                seg.is_tip = false;
            }
        }
    }
    
    // Add new segments
    for (const auto& new_seg : new_segments) {
        if (segments_.size() < MAX_SEGMENTS) {
            segments_.push_back(new_seg);
        }
    }
}

bool MandelbrotRootVeinsEffect::segmentsIntersect(
    const cv::Point2f& p1, const cv::Point2f& p2,
    const cv::Point2f& p3, const cv::Point2f& p4,
    cv::Point2f& intersection) {
    return false;  // Unused
}

void MandelbrotRootVeinsEffect::checkIntersections() {
    // Unused
}

void MandelbrotRootVeinsEffect::spawnBranch(const cv::Point2f& origin, float base_direction, int parent_generation) {
    // Unused - branching handled in growVeins
}

void MandelbrotRootVeinsEffect::updateWilting(float dt) {
    // Start wilting when near capacity
    if (segments_.size() > MAX_SEGMENTS * 0.85f) {
        for (auto& seg : segments_) {
            if (!seg.is_tip && !seg.is_wilting && seg.generation > 2 && seg.age > 2.0f) {
                seg.is_wilting = true;
                break;
            }
        }
    }
    
    for (auto& seg : segments_) {
        if (seg.is_wilting) {
            seg.wilt_progress += WILT_SPEED * dt * 30.0f;
        }
    }

    // Cleanup fully wilted
    static int cleanup_counter = 0;
    if (++cleanup_counter >= 90) {
        cleanup_counter = 0;
        segments_.erase(
            std::remove_if(segments_.begin(), segments_.end(),
                [](const VeinSegment& s) { return s.wilt_progress >= 1.0f && s.generation > 0; }),
            segments_.end());
    }
}

cv::Point2f MandelbrotRootVeinsEffect::applyZoomRotation(const cv::Point2f& p) {
    return p;  // No transform
}

float MandelbrotRootVeinsEffect::getSegmentBrightness(const VeinSegment& seg) {
    float brightness = 0.9f + 0.1f * sin(seg.age * 3.0f + seg.phase);
    float gen_fade = 1.0f - seg.generation * 0.1f;
    gen_fade = std::max(0.4f, gen_fade);
    float wilt_fade = 1.0f - seg.wilt_progress;
    return brightness * gen_fade * wilt_fade;
}

void MandelbrotRootVeinsEffect::renderVeins(cv::Mat& frame) {
    frame = cv::Mat::zeros(frame.rows, frame.cols, CV_8UC3);
    
    int w = frame.cols;
    int h = frame.rows;
    
    // Draw segments as smooth curves using collected points per vein chain
    // For now, draw individual segments with anti-aliasing
    
    for (const auto& seg : segments_) {
        if (seg.wilt_progress >= 1.0f) continue;
        
        // Skip zero-length segments
        float len = sqrt((seg.end.x - seg.start.x) * (seg.end.x - seg.start.x) +
                         (seg.end.y - seg.start.y) * (seg.end.y - seg.start.y));
        if (len < 0.001f) continue;
        
        // Convert normalized coordinates to pixel coordinates
        cv::Point p1(static_cast<int>(seg.start.x * w), static_cast<int>(seg.start.y * h));
        cv::Point p2(static_cast<int>(seg.end.x * w), static_cast<int>(seg.end.y * h));
        
        float brightness = getSegmentBrightness(seg);
        
        // Color: gradient from electric blue (roots) to magenta (tips)
        float t = seg.generation / static_cast<float>(MAX_GENERATION);
        int b = static_cast<int>((255 - t * 100) * brightness);  // Blue decreases
        int g = static_cast<int>((50 + t * 50) * brightness);    // Green stays low
        int r = static_cast<int>((100 + t * 155) * brightness);  // Red increases
        
        cv::Scalar color(b, g, r);
        
        // Thinner lines for fractal look - thickness 1 for all
        cv::line(frame, p1, p2, color, 1, cv::LINE_AA);
    }
    
    // Draw glowing tips
    for (const auto& seg : segments_) {
        if (seg.is_tip && !seg.is_wilting) {
            cv::Point tip(static_cast<int>(seg.end.x * w), static_cast<int>(seg.end.y * h));
            float brightness = getSegmentBrightness(seg);
            cv::Scalar glow_color(
                static_cast<int>(255 * brightness),
                static_cast<int>(200 * brightness),
                static_cast<int>(255 * brightness)
            );
            cv::circle(frame, tip, 2, glow_color, -1, cv::LINE_AA);
        }
    }
}

void MandelbrotRootVeinsEffect::process(cv::Mat& out_bgr, int target_width, int target_height) {
    if (width_ <= 0 || height_ <= 0) {
        out_bgr = cv::Mat::zeros(64, 64, CV_8UC3);
        return;
    }

    int output_width = (target_width > 0) ? target_width : width_;
    int output_height = (target_height > 0) ? target_height : height_;

    if (output_width <= 0 || output_height <= 0) {
        output_width = 64;
        output_height = 64;
    }

    float dt = 1.0f / 30.0f;
    time_ += dt;

    // Grow veins
    growVeins(dt);
    
    // Manage wilting
    updateWilting(dt);
    
    // Check if we need to reset (no active tips)
    int active_tips = 0;
    for (const auto& seg : segments_) {
        if (seg.is_tip && !seg.is_wilting) active_tips++;
    }
    
    static float no_tips_time = 0.0f;
    if (active_tips == 0) {
        no_tips_time += dt;
        if (no_tips_time > 2.0f) {  // Reset after 2 seconds of no growth
            reset();
            no_tips_time = 0.0f;
        }
    } else {
        no_tips_time = 0.0f;
    }

    try {
        out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);
        renderVeins(out_bgr);
        
        // Soft glow
        cv::Mat glow;
        cv::GaussianBlur(out_bgr, glow, cv::Size(3, 3), 1.0);
        cv::addWeighted(out_bgr, 0.8, glow, 0.4, 0, out_bgr);
        
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error: " << e.what() << std::endl;
        out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);
    }
}
