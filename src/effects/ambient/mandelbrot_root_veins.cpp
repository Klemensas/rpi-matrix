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
    
    // All 4 corners with angles pointing toward center
    struct CornerStart {
        float x, y;
        float angle;
    };
    
    CornerStart corners[4] = {
        {0.0f, 0.0f, static_cast<float>(atan2(0.5f, 0.5f))},
        {1.0f, 0.0f, static_cast<float>(atan2(0.5f, -0.5f))},
        {0.0f, 1.0f, static_cast<float>(atan2(-0.5f, 0.5f))},
        {1.0f, 1.0f, static_cast<float>(atan2(-0.5f, -0.5f))}
    };
    
    // Create 8 veins per corner for dense network
    for (int corner = 0; corner < 4; corner++) {
        for (int vein = 0; vein < 8; vein++) {
            VeinSegment root;
            float offset = 0.01f;
            root.start.x = corners[corner].x + (corners[corner].x < 0.5f ? offset : -offset);
            root.start.y = corners[corner].y + (corners[corner].y < 0.5f ? offset : -offset);
            root.end = root.start;
            root.age = 0.0f;
            root.generation = 0;
            root.is_wilting = false;
            root.wilt_progress = 0.0f;
            root.phase = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
            // Wide spread: veins fan out from -0.7 to +0.7 radians (~80 degrees total)
            float spread = (vein - 3.5f) * 0.2f;  // Range: -0.7 to +0.7
            root.direction = corners[corner].angle + spread;
            root.is_tip = true;
            segments_.push_back(root);
        }
    }
}

// Simple noise function for organic variation
float MandelbrotRootVeinsEffect::noise(float x, float y, float z) {
    // Combine multiple sine waves for pseudo-random organic movement
    float n = sin(x * 12.9898f + y * 78.233f + z * 37.719f);
    n = sin(n * 43758.5453f);
    return n;
}

float MandelbrotRootVeinsEffect::getMandelbrotDirection(float x, float y, float base_angle) {
    // Map to Mandelbrot space
    float mx = x * 4.0f - 2.0f;
    float my = y * 4.0f - 2.0f;
    
    float zx = 0.0f, zy = 0.0f;
    int max_iter = 8;
    for (int i = 0; i < max_iter; i++) {
        float new_zx = zx * zx - zy * zy + mx;
        float new_zy = 2.0f * zx * zy + my;
        zx = new_zx;
        zy = new_zy;
        if (zx * zx + zy * zy > 4.0f) break;
    }
    
    float escape_angle = atan2(zy, zx);
    return base_angle + 0.25f * sin(escape_angle * 1.5f + time_ * 0.3f);
}

void MandelbrotRootVeinsEffect::growVeins(float dt) {
    const float center_x = 0.5f;
    const float center_y = 0.5f;
    
    std::vector<VeinSegment> new_segments;
    
    for (auto& seg : segments_) {
        if (!seg.is_tip || seg.is_wilting) continue;
        
        seg.age += dt;
        
        // Direction toward center
        float to_center = atan2(center_y - seg.end.y, center_x - seg.end.x);
        
        // Strong organic curvature using layered sine waves
        // This creates the "vein-like" waviness
        float wave1 = 0.4f * sin(seg.age * 8.0f + seg.phase);
        float wave2 = 0.25f * sin(seg.age * 12.0f + seg.phase * 1.7f + seg.end.x * 20.0f);
        float wave3 = 0.15f * cos(seg.age * 6.0f + seg.phase * 2.3f + seg.end.y * 15.0f);
        float wave4 = 0.1f * sin(time_ * 3.0f + seg.end.x * 30.0f + seg.end.y * 25.0f);
        
        // Noise-based variation for unpredictability
        float noise_val = noise(seg.end.x * 10.0f, seg.end.y * 10.0f, time_ * 0.5f);
        float noise_curve = 0.2f * noise_val;
        
        // Combine base direction with waves
        float blend = 0.7f;  // How much to follow previous direction vs toward center
        float base_dir = seg.direction * blend + to_center * (1.0f - blend);
        
        // Add all the curvature
        float dir = base_dir + wave1 + wave2 + wave3 + wave4 + noise_curve;
        
        // Add Mandelbrot influence
        dir = getMandelbrotDirection(seg.end.x, seg.end.y, dir);
        
        // Very small growth steps for smooth curves
        float speed_factor = 1.0f / (1.0f + seg.generation * 0.3f);
        float growth = 0.003f * speed_factor;
        
        cv::Point2f new_end;
        new_end.x = seg.end.x + growth * cos(dir);
        new_end.y = seg.end.y + growth * sin(dir);
        
        // Bounds check
        float dist_to_center = sqrt((new_end.x - center_x) * (new_end.x - center_x) + 
                                    (new_end.y - center_y) * (new_end.y - center_y));
        
        if (dist_to_center < 0.05f ||
            new_end.x < -0.02f || new_end.x > 1.02f ||
            new_end.y < -0.02f || new_end.y > 1.02f) {
            seg.is_tip = false;
            continue;
        }
        
        seg.end = new_end;
        seg.direction = dir;
        
        // Segment length
        float seg_length = sqrt((seg.end.x - seg.start.x) * (seg.end.x - seg.start.x) +
                                (seg.end.y - seg.start.y) * (seg.end.y - seg.start.y));
        
        // Very short segments for smooth appearance, frequent branching
        float branch_length = 0.015f + seg.generation * 0.005f;
        
        if (seg_length > branch_length && segments_.size() + new_segments.size() < MAX_SEGMENTS) {
            
            // High branch probability
            float branch_prob = 0.6f - seg.generation * 0.06f;
            branch_prob = std::max(0.1f, branch_prob);
            
            bool should_branch = (static_cast<float>(rand()) / RAND_MAX < branch_prob) && 
                                 (seg.generation < MAX_GENERATION);
            
            if (should_branch) {
                // Y-split branching
                for (int b = 0; b < 2; b++) {
                    VeinSegment branch;
                    branch.start = seg.end;
                    branch.end = seg.end;
                    branch.age = 0.0f;
                    branch.generation = seg.generation + 1;
                    branch.is_wilting = false;
                    branch.wilt_progress = 0.0f;
                    branch.phase = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
                    
                    // Asymmetric angles
                    float angle_var = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.25f;
                    float branch_angle = (b == 0) ? 
                        (BRANCH_ANGLE_SPREAD * 0.6f + angle_var) : 
                        (-BRANCH_ANGLE_SPREAD * 0.6f + angle_var);
                    branch.direction = dir + branch_angle;
                    branch.is_tip = true;
                    
                    new_segments.push_back(branch);
                }
                seg.is_tip = false;
            } else {
                // Continue as new segment
                VeinSegment continuation;
                continuation.start = seg.end;
                continuation.end = seg.end;
                continuation.age = seg.age;  // Keep age for continuous waves
                continuation.generation = seg.generation;
                continuation.is_wilting = false;
                continuation.wilt_progress = 0.0f;
                continuation.phase = seg.phase;  // Keep phase for smooth waves
                continuation.direction = dir;
                continuation.is_tip = true;
                
                new_segments.push_back(continuation);
                seg.is_tip = false;
            }
        }
    }
    
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
    return false;
}

void MandelbrotRootVeinsEffect::checkIntersections() {}

void MandelbrotRootVeinsEffect::spawnBranch(const cv::Point2f& origin, float base_direction, int parent_generation) {}

void MandelbrotRootVeinsEffect::updateWilting(float dt) {
    if (segments_.size() > MAX_SEGMENTS * 0.9f) {
        for (auto& seg : segments_) {
            if (!seg.is_tip && !seg.is_wilting && seg.generation > 3 && seg.age > 1.5f) {
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

    static int cleanup_counter = 0;
    if (++cleanup_counter >= 60) {
        cleanup_counter = 0;
        segments_.erase(
            std::remove_if(segments_.begin(), segments_.end(),
                [](const VeinSegment& s) { return s.wilt_progress >= 1.0f && s.generation > 0; }),
            segments_.end());
    }
}

cv::Point2f MandelbrotRootVeinsEffect::applyZoomRotation(const cv::Point2f& p) {
    return p;
}

float MandelbrotRootVeinsEffect::getSegmentBrightness(const VeinSegment& seg) {
    float brightness = 0.85f + 0.15f * sin(seg.age * 2.0f + seg.phase);
    float gen_fade = 1.0f - seg.generation * 0.08f;
    gen_fade = std::max(0.35f, gen_fade);
    float wilt_fade = 1.0f - seg.wilt_progress;
    return brightness * gen_fade * wilt_fade;
}

void MandelbrotRootVeinsEffect::renderVeins(cv::Mat& frame) {
    frame = cv::Mat::zeros(frame.rows, frame.cols, CV_8UC3);
    
    int w = frame.cols;
    int h = frame.rows;
    
    // Draw all segments
    for (const auto& seg : segments_) {
        if (seg.wilt_progress >= 1.0f) continue;
        
        float len = sqrt((seg.end.x - seg.start.x) * (seg.end.x - seg.start.x) +
                         (seg.end.y - seg.start.y) * (seg.end.y - seg.start.y));
        if (len < 0.0005f) continue;
        
        cv::Point p1(static_cast<int>(seg.start.x * w), static_cast<int>(seg.start.y * h));
        cv::Point p2(static_cast<int>(seg.end.x * w), static_cast<int>(seg.end.y * h));
        
        float brightness = getSegmentBrightness(seg);
        
        // Color gradient: cyan -> blue -> purple -> magenta
        float t = seg.generation / static_cast<float>(MAX_GENERATION);
        int b = static_cast<int>((220 - t * 50) * brightness);
        int g = static_cast<int>((180 - t * 140) * brightness);
        int r = static_cast<int>((80 + t * 175) * brightness);
        
        cv::Scalar color(b, g, r);
        cv::line(frame, p1, p2, color, 1, cv::LINE_AA);
    }
    
    // Glowing tips
    for (const auto& seg : segments_) {
        if (seg.is_tip && !seg.is_wilting) {
            cv::Point tip(static_cast<int>(seg.end.x * w), static_cast<int>(seg.end.y * h));
            float brightness = getSegmentBrightness(seg);
            cv::Scalar glow(
                static_cast<int>(255 * brightness),
                static_cast<int>(180 * brightness),
                static_cast<int>(255 * brightness)
            );
            cv::circle(frame, tip, 1, glow, -1, cv::LINE_AA);
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

    growVeins(dt);
    updateWilting(dt);
    
    // Reset check
    int active_tips = 0;
    for (const auto& seg : segments_) {
        if (seg.is_tip && !seg.is_wilting) active_tips++;
    }
    
    static float no_tips_time = 0.0f;
    if (active_tips == 0) {
        no_tips_time += dt;
        if (no_tips_time > 2.0f) {
            reset();
            no_tips_time = 0.0f;
        }
    } else {
        no_tips_time = 0.0f;
    }

    try {
        out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);
        renderVeins(out_bgr);
        
        // Subtle glow
        cv::Mat glow;
        cv::GaussianBlur(out_bgr, glow, cv::Size(3, 3), 0.8);
        cv::addWeighted(out_bgr, 0.85, glow, 0.35, 0, out_bgr);
        
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error: " << e.what() << std::endl;
        out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);
    }
}
