#include "effects/ambient/oval_chain.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <opencv2/imgproc.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

OvalChainEffect::OvalChainEffect(int width, int height)
    : width_(width), height_(height), time_(0.0f), next_link_id_(0),
      last_trail_spawn_time_(0.0f), cycle_start_time_(0.0f),
      current_direction_(ChainDirection::FROM_LEFT) {
    
    // Initialize with a random direction
    startNewChain();
}

void OvalChainEffect::startNewChain() {
    // Pick a random direction
    int dir = rand() % 4;
    current_direction_ = static_cast<ChainDirection>(dir);
    
    // Clear old trails
    trail_links_.clear();
    
    // Set up starting position and rotation based on direction
    float start_x, start_y, base_rotation;
    
    switch (current_direction_) {
        case ChainDirection::FROM_LEFT:
            start_x = -LINK_OUTER_WIDTH;
            start_y = height_ / 2.0f;
            base_rotation = 0.0f;  // Horizontal orientation
            break;
        case ChainDirection::FROM_RIGHT:
            start_x = width_ + LINK_OUTER_WIDTH;
            start_y = height_ / 2.0f;
            base_rotation = 0.0f;  // Horizontal orientation
            break;
        case ChainDirection::FROM_TOP:
            start_x = width_ / 2.0f;
            start_y = -LINK_OUTER_WIDTH;
            base_rotation = M_PI / 2.0f;  // Vertical orientation
            break;
        case ChainDirection::FROM_BOTTOM:
            start_x = width_ / 2.0f;
            start_y = height_ + LINK_OUTER_WIDTH;
            base_rotation = M_PI / 2.0f;  // Vertical orientation
            break;
    }
    
    // Initialize the active link
    active_link_.id = next_link_id_++;
    active_link_.position = cv::Point2f(start_x, start_y);
    active_link_.rotation = base_rotation;
    active_link_.brightness = BASE_BRIGHTNESS;
    active_link_.z_order = 100;
    active_link_.age = 0.0f;
    active_link_.is_active = true;
    active_link_.is_threading = false;
    active_link_.threading_with_id = -1;
    active_link_.threading_depth = 0.0f;
    active_link_.velocity = cv::Point2f(0, 0);
    active_link_.oscillation_phase = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
    
    cycle_start_time_ = time_;
    last_trail_spawn_time_ = time_;
}

void OvalChainEffect::process(cv::Mat& out_bgr, int target_width, int target_height) {
    if (width_ <= 0 || height_ <= 0) {
        out_bgr = cv::Mat::zeros(480, 640, CV_8UC3);
        return;
    }

    int output_width = (target_width > 0) ? target_width : width_;
    int output_height = (target_height > 0) ? target_height : height_;

    if (output_width <= 0 || output_height <= 0) {
        output_width = 640;
        output_height = 480;
    }

    float dt = 1.0f / 30.0f;
    time_ += dt;

    try {
        out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);

        updateChain(dt);
        updateTrails(dt);
        detectThreading();
        renderChain(out_bgr);

    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error in oval chain effect: " << e.what() << std::endl;
        out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);
    }
}

void OvalChainEffect::updateChain(float dt) {
    float cycle_time = time_ - cycle_start_time_;
    float progress = cycle_time / TRAVERSE_TIME;
    
    // Check if cycle is complete
    if (progress >= 1.0f) {
        startNewChain();
        return;
    }
    
    // Calculate position based on direction
    float base_x, base_y;
    float oscillation_offset;
    
    // Calculate travel distance including off-screen margins
    float horizontal_travel = width_ + LINK_OUTER_WIDTH * 2;
    float vertical_travel = height_ + LINK_OUTER_WIDTH * 2;
    
    switch (current_direction_) {
        case ChainDirection::FROM_LEFT:
            oscillation_offset = OSCILLATION_AMPLITUDE * sin(progress * OSCILLATION_FREQUENCY * 2.0f * M_PI + active_link_.oscillation_phase);
            base_x = -LINK_OUTER_WIDTH + progress * horizontal_travel;
            base_y = height_ / 2.0f + oscillation_offset;
            break;
            
        case ChainDirection::FROM_RIGHT:
            oscillation_offset = OSCILLATION_AMPLITUDE * sin(progress * OSCILLATION_FREQUENCY * 2.0f * M_PI + active_link_.oscillation_phase);
            base_x = width_ + LINK_OUTER_WIDTH - progress * horizontal_travel;
            base_y = height_ / 2.0f + oscillation_offset;
            break;
            
        case ChainDirection::FROM_TOP:
            oscillation_offset = OSCILLATION_AMPLITUDE * sin(progress * OSCILLATION_FREQUENCY * 2.0f * M_PI + active_link_.oscillation_phase);
            base_x = width_ / 2.0f + oscillation_offset;
            base_y = -LINK_OUTER_WIDTH + progress * vertical_travel;
            break;
            
        case ChainDirection::FROM_BOTTOM:
            oscillation_offset = OSCILLATION_AMPLITUDE * sin(progress * OSCILLATION_FREQUENCY * 2.0f * M_PI + active_link_.oscillation_phase);
            base_x = width_ / 2.0f + oscillation_offset;
            base_y = height_ + LINK_OUTER_WIDTH - progress * vertical_travel;
            break;
    }
    
    cv::Point2f prev_pos = active_link_.position;
    active_link_.position = cv::Point2f(base_x, base_y);
    active_link_.velocity = (active_link_.position - prev_pos) / dt;
    
    // Rotate based on velocity direction for chain-like interlock appearance
    if (cv::norm(active_link_.velocity) > 0.1f) {
        float target_rotation = atan2(active_link_.velocity.y, active_link_.velocity.x);
        // Smooth rotation transition
        float rot_diff = target_rotation - active_link_.rotation;
        // Normalize to -PI to PI
        while (rot_diff > M_PI) rot_diff -= 2.0f * M_PI;
        while (rot_diff < -M_PI) rot_diff += 2.0f * M_PI;
        active_link_.rotation += rot_diff * 0.15f;  // Smooth follow
    }
    
    // Add slight wobble for organic feel
    active_link_.rotation += sin(time_ * 3.0f) * 0.02f;
    
    // Spawn trail links at regular intervals
    if (time_ - last_trail_spawn_time_ >= TRAIL_SPAWN_INTERVAL) {
        spawnTrailLink();
        last_trail_spawn_time_ = time_;
    }
}

void OvalChainEffect::spawnTrailLink() {
    OvalLink trail;
    trail.id = next_link_id_++;
    trail.position = active_link_.position;
    
    // Alternate rotation slightly for interlocking chain appearance
    // Every other link rotates perpendicular for realistic chain look
    int link_index = static_cast<int>(trail_links_.size());
    if (link_index % 2 == 0) {
        trail.rotation = active_link_.rotation;
    } else {
        // Perpendicular rotation for interlocking effect
        trail.rotation = active_link_.rotation + M_PI / 2.0f;
    }
    
    trail.brightness = BASE_BRIGHTNESS * 0.95f;
    trail.z_order = link_index;
    trail.age = 0.0f;
    trail.is_active = false;
    trail.is_threading = false;
    trail.threading_with_id = -1;
    trail.threading_depth = 0.0f;
    trail.velocity = cv::Point2f(0, 0);
    trail.oscillation_phase = 0.0f;
    
    trail_links_.push_back(trail);
    
    while (trail_links_.size() > MAX_TRAIL_LINKS) {
        trail_links_.erase(trail_links_.begin());
    }
}

void OvalChainEffect::updateTrails(float dt) {
    for (auto& link : trail_links_) {
        link.age += dt;
        
        float fade_progress = std::min(link.age / TRAIL_FADE_TIME, 1.0f);
        float base_fade = 1.0f - fade_progress;
        base_fade = base_fade * base_fade;
        
        link.brightness = BASE_BRIGHTNESS * base_fade;
        
        if (link.brightness < MIN_TRAIL_BRIGHTNESS) {
            link.brightness = MIN_TRAIL_BRIGHTNESS;
        }
    }
    
    auto it = std::remove_if(trail_links_.begin(), trail_links_.end(),
        [](const OvalLink& link) {
            return link.age > TRAIL_FADE_TIME * 1.2f;
        });
    trail_links_.erase(it, trail_links_.end());
}

void OvalChainEffect::detectThreading() {
    active_link_.is_threading = false;
    active_link_.threading_with_id = -1;
    active_link_.threading_depth = 0.0f;
    
    for (auto& link : trail_links_) {
        if (!link.is_threading) {
            link.threading_with_id = -1;
            link.threading_depth = 0.0f;
        }
    }
    
    for (auto& trail_link : trail_links_) {
        if (trail_link.age < 0.3f || trail_link.brightness < MIN_TRAIL_BRIGHTNESS * 1.5f) {
            continue;
        }
        
        if (checkThreadingCondition(active_link_, trail_link)) {
            active_link_.is_threading = true;
            active_link_.threading_with_id = trail_link.id;
            active_link_.threading_depth = calculateThreadingDepth(active_link_, trail_link);
            
            trail_link.is_threading = true;
            trail_link.threading_with_id = active_link_.id;
            trail_link.threading_depth = active_link_.threading_depth;
            
            trail_link.brightness = std::min(1.2f, trail_link.brightness + THREADING_BRIGHTNESS_BOOST);
            trail_link.z_order = 50;
            
            break;
        } else if (trail_link.threading_with_id == active_link_.id) {
            trail_link.is_threading = false;
            trail_link.threading_with_id = -1;
            trail_link.z_order = static_cast<int>(&trail_link - &trail_links_[0]);
        }
    }
}

bool OvalChainEffect::isPointInEllipseHole(const cv::Point2f& point, const OvalLink& ring) {
    float dx = point.x - ring.position.x;
    float dy = point.y - ring.position.y;
    
    float cos_r = cos(-ring.rotation);
    float sin_r = sin(-ring.rotation);
    float local_x = dx * cos_r - dy * sin_r;
    float local_y = dx * sin_r + dy * cos_r;
    
    float hole_width = LINK_OUTER_WIDTH * HOLE_RATIO * 0.5f;
    float hole_height = LINK_OUTER_HEIGHT * HOLE_RATIO * 0.5f;
    
    float normalized_x = local_x / hole_width;
    float normalized_y = local_y / hole_height;
    
    return (normalized_x * normalized_x + normalized_y * normalized_y) < 1.0f;
}

bool OvalChainEffect::checkThreadingCondition(const OvalLink& moving, const OvalLink& stationary) {
    float dx = moving.position.x - stationary.position.x;
    float dy = moving.position.y - stationary.position.y;
    float distance = sqrt(dx * dx + dy * dy);
    
    float max_distance = LINK_OUTER_WIDTH * 0.7f;
    float min_distance = LINK_OUTER_WIDTH * 0.08f;
    
    if (distance > max_distance || distance < min_distance) {
        return false;
    }
    
    return isPointInEllipseHole(moving.position, stationary);
}

float OvalChainEffect::calculateThreadingDepth(const OvalLink& moving, const OvalLink& stationary) {
    float dx = moving.position.x - stationary.position.x;
    float dy = moving.position.y - stationary.position.y;
    float distance = sqrt(dx * dx + dy * dy);
    
    float max_dist = LINK_OUTER_WIDTH * HOLE_RATIO * 0.5f;
    float depth = 1.0f - (distance / max_dist);
    
    return std::max(0.0f, std::min(1.0f, depth));
}

void OvalChainEffect::renderChain(cv::Mat& frame) {
    std::vector<OvalLink*> all_links;
    
    for (auto& link : trail_links_) {
        all_links.push_back(&link);
    }
    all_links.push_back(&active_link_);
    
    std::sort(all_links.begin(), all_links.end(),
        [](const OvalLink* a, const OvalLink* b) {
            return a->z_order < b->z_order;
        });
    
    for (const auto* link : all_links) {
        if (link->brightness >= MIN_TRAIL_BRIGHTNESS) {
            drawMetallicRing(frame, *link);
        }
    }
}

void OvalChainEffect::drawMetallicRing(cv::Mat& frame, const OvalLink& link) {
    cv::Point center(static_cast<int>(link.position.x), static_cast<int>(link.position.y));
    
    // Skip if completely off-screen
    float max_dim = std::max(LINK_OUTER_WIDTH, LINK_OUTER_HEIGHT);
    if (center.x < -max_dim || center.x > frame.cols + max_dim ||
        center.y < -max_dim || center.y > frame.rows + max_dim) {
        return;
    }
    
    float angle_deg = link.rotation * 180.0f / M_PI;
    float brightness = link.brightness;
    
    if (link.is_threading) {
        brightness = std::min(1.3f, brightness + THREADING_BRIGHTNESS_BOOST * link.threading_depth);
    }
    
    // Outer ellipse dimensions (elongated chain link)
    cv::Size outer_size(static_cast<int>(LINK_OUTER_WIDTH), static_cast<int>(LINK_OUTER_HEIGHT));
    
    // Inner hole dimensions
    int inner_width = static_cast<int>(LINK_OUTER_WIDTH * HOLE_RATIO);
    int inner_height = static_cast<int>(LINK_OUTER_HEIGHT * HOLE_RATIO);
    cv::Size inner_size(inner_width, inner_height);
    
    // === Metallic color scheme (silver/chrome) ===
    float base_intensity = brightness * 200.0f;
    cv::Scalar base_color(
        base_intensity * 0.82f,   // B - cool metallic
        base_intensity * 0.88f,   // G
        base_intensity * 0.92f    // R - slight warmth
    );
    
    // Shadow for 3D depth
    float shadow_intensity = brightness * 90.0f;
    cv::Scalar shadow_color(
        shadow_intensity * 0.65f,
        shadow_intensity * 0.70f,
        shadow_intensity * 0.75f
    );
    
    // Specular highlight
    float highlight_intensity = std::min(255.0f, brightness * 280.0f);
    cv::Scalar highlight_color(
        std::min(255.0f, highlight_intensity * 0.92f),
        std::min(255.0f, highlight_intensity * 0.95f),
        std::min(255.0f, highlight_intensity * 1.0f)
    );
    
    // === Draw the elongated ring ===
    
    // 1. Draw outer ellipse (main body)
    cv::ellipse(frame, center, outer_size, angle_deg, 0, 360, base_color, -1, cv::LINE_AA);
    
    // 2. Draw shadow arc on lower edge for 3D effect
    float shadow_offset_x = 1.5f * cos(link.rotation + M_PI * 0.6f);
    float shadow_offset_y = 1.5f * sin(link.rotation + M_PI * 0.6f);
    cv::Point shadow_center(
        center.x + static_cast<int>(shadow_offset_x),
        center.y + static_cast<int>(shadow_offset_y)
    );
    cv::ellipse(frame, shadow_center, 
                cv::Size(outer_size.width - 1, outer_size.height - 1),
                angle_deg, 120, 300, shadow_color, 2, cv::LINE_AA);
    
    // 3. Draw inner hole (black for chain appearance)
    cv::Scalar hole_color(3, 3, 3);
    cv::ellipse(frame, center, inner_size, angle_deg, 0, 360, hole_color, -1, cv::LINE_AA);
    
    // 4. Draw highlight arc on upper edge
    float highlight_offset_x = 1.0f * cos(link.rotation - M_PI * 0.3f);
    float highlight_offset_y = 1.0f * sin(link.rotation - M_PI * 0.3f);
    cv::Point highlight_center(
        center.x + static_cast<int>(highlight_offset_x),
        center.y + static_cast<int>(highlight_offset_y)
    );
    cv::ellipse(frame, highlight_center,
                cv::Size(outer_size.width - 2, outer_size.height - 2),
                angle_deg, -50, 50, highlight_color, 2, cv::LINE_AA);
    
    // 5. Inner edge highlight for depth
    cv::ellipse(frame, center, 
                cv::Size(inner_size.width + 1, inner_size.height + 1),
                angle_deg, -70, 70, highlight_color, 1, cv::LINE_AA);
    
    // 6. Outer edge definition
    cv::Scalar edge_color(
        base_intensity * 0.45f,
        base_intensity * 0.50f,
        base_intensity * 0.55f
    );
    cv::ellipse(frame, center, outer_size, angle_deg, 0, 360, edge_color, 1, cv::LINE_AA);
    
    // 7. Add subtle cross-bar effect for chain link appearance (drawn as a subtle line across)
    if (brightness > 0.4f) {
        float bar_length = LINK_OUTER_WIDTH * 0.35f;
        float bar_angle = link.rotation;
        cv::Point bar_start(
            center.x - static_cast<int>(bar_length * cos(bar_angle)),
            center.y - static_cast<int>(bar_length * sin(bar_angle))
        );
        cv::Point bar_end(
            center.x + static_cast<int>(bar_length * cos(bar_angle)),
            center.y + static_cast<int>(bar_length * sin(bar_angle))
        );
        cv::Scalar bar_color(
            shadow_intensity * 0.5f,
            shadow_intensity * 0.55f,
            shadow_intensity * 0.6f
        );
        // Draw subtle inner structure line
        cv::line(frame, bar_start, bar_end, bar_color, 1, cv::LINE_AA);
    }
}

void OvalChainEffect::drawOvalLink(cv::Mat& frame, const OvalLink& link) {
    drawMetallicRing(frame, link);
}
