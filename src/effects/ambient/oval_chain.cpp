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
    
    // Clear old chain
    trail_links_.clear();
    
    // Create the full chain with proper interlocking links
    buildInterlockingChain();
    
    cycle_start_time_ = time_;
    last_trail_spawn_time_ = time_;
}

void OvalChainEffect::buildInterlockingChain() {
    // Calculate chain parameters based on direction
    bool is_horizontal = (current_direction_ == ChainDirection::FROM_LEFT || 
                          current_direction_ == ChainDirection::FROM_RIGHT);
    
    // Link spacing - distance between centers of adjacent links
    // For proper interlocking, links need to be close enough that one passes through the other's hole
    float link_spacing = LINK_OUTER_WIDTH * 0.55f;  // Tight spacing for interlocking
    
    // Calculate total chain length needed to span screen plus margins
    float total_travel = is_horizontal ? (width_ + LINK_OUTER_WIDTH * 4) : (height_ + LINK_OUTER_WIDTH * 4);
    int num_links = static_cast<int>(total_travel / link_spacing) + 2;
    num_links = std::min(num_links, MAX_TRAIL_LINKS);
    
    // Starting position (off-screen)
    float start_x, start_y;
    float direction_sign = 1.0f;  // Direction of movement
    
    switch (current_direction_) {
        case ChainDirection::FROM_LEFT:
            start_x = -LINK_OUTER_WIDTH * 2;
            start_y = height_ / 2.0f;
            direction_sign = 1.0f;
            break;
        case ChainDirection::FROM_RIGHT:
            start_x = width_ + LINK_OUTER_WIDTH * 2;
            start_y = height_ / 2.0f;
            direction_sign = -1.0f;
            break;
        case ChainDirection::FROM_TOP:
            start_x = width_ / 2.0f;
            start_y = -LINK_OUTER_WIDTH * 2;
            direction_sign = 1.0f;
            break;
        case ChainDirection::FROM_BOTTOM:
            start_x = width_ / 2.0f;
            start_y = height_ + LINK_OUTER_WIDTH * 2;
            direction_sign = -1.0f;
            break;
    }
    
    // Build the chain
    for (int i = 0; i < num_links; i++) {
        OvalLink link;
        link.id = next_link_id_++;
        
        // Position along the chain
        float offset = i * link_spacing * direction_sign;
        if (is_horizontal) {
            link.position = cv::Point2f(start_x + offset, start_y);
        } else {
            link.position = cv::Point2f(start_x, start_y + offset);
        }
        
        // Alternate rotation for interlocking
        // Even links: along chain direction, Odd links: perpendicular (through previous link's hole)
        if (is_horizontal) {
            link.rotation = (i % 2 == 0) ? 0.0f : M_PI / 2.0f;
        } else {
            link.rotation = (i % 2 == 0) ? M_PI / 2.0f : 0.0f;
        }
        
        link.brightness = BASE_BRIGHTNESS;
        link.z_order = i;  // Will be adjusted for 3D rendering
        link.age = 0.0f;
        link.is_active = (i == 0);  // First link is the "head"
        link.is_threading = true;  // All links are interlocked
        link.threading_with_id = (i > 0) ? trail_links_[i-1].id : -1;
        link.threading_depth = 0.5f;  // Fully threaded
        link.velocity = cv::Point2f(0, 0);
        link.oscillation_phase = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
        
        trail_links_.push_back(link);
    }
    
    // Set the active link to the first one
    if (!trail_links_.empty()) {
        active_link_ = trail_links_[0];
        active_link_.is_active = true;
    }
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
        renderChain(out_bgr);

    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error in oval chain effect: " << e.what() << std::endl;
        out_bgr = cv::Mat::zeros(output_height, output_width, CV_8UC3);
    }
}

void OvalChainEffect::updateChain(float dt) {
    (void)dt;  // Delta time not needed - we use absolute progress
    
    float cycle_time = time_ - cycle_start_time_;
    float progress = cycle_time / TRAVERSE_TIME;
    
    // Check if cycle is complete (chain has fully exited screen)
    if (progress >= 1.0f) {
        startNewChain();
        return;
    }
    
    bool is_horizontal = (current_direction_ == ChainDirection::FROM_LEFT || 
                          current_direction_ == ChainDirection::FROM_RIGHT);
    
    // Link spacing for interlocking
    float link_spacing = LINK_OUTER_WIDTH * 0.55f;
    
    // Calculate chain length
    float chain_length = trail_links_.size() * link_spacing;
    
    // Total travel = start margin + screen + chain length + end margin
    // The chain needs to fully enter and fully exit the screen
    float screen_size = is_horizontal ? static_cast<float>(width_) : static_cast<float>(height_);
    float margin = LINK_OUTER_WIDTH * 2;
    float total_travel = margin + screen_size + chain_length + margin;
    
    // Current position of the first link (head of chain)
    float head_offset = progress * total_travel - margin;
    
    // Direction multiplier
    float dir_mult = 1.0f;
    if (current_direction_ == ChainDirection::FROM_RIGHT || 
        current_direction_ == ChainDirection::FROM_BOTTOM) {
        dir_mult = -1.0f;
    }
    
    // Update all link positions - the entire chain moves together
    for (size_t i = 0; i < trail_links_.size(); i++) {
        OvalLink& link = trail_links_[i];
        
        // Each link trails behind the previous one
        float link_pos = head_offset - i * link_spacing;
        
        // Add wave motion for organic chain movement
        float wave_offset = OSCILLATION_AMPLITUDE * sin(time_ * 2.5f + i * 0.4f);
        
        if (is_horizontal) {
            float start_x = (current_direction_ == ChainDirection::FROM_LEFT) ? 0.0f : static_cast<float>(width_);
            link.position.x = start_x + link_pos * dir_mult;
            link.position.y = height_ / 2.0f + wave_offset;
            
            // Alternate rotation: 0 (horizontal) and PI/2 (vertical)
            link.rotation = (i % 2 == 0) ? 0.0f : M_PI / 2.0f;
        } else {
            float start_y = (current_direction_ == ChainDirection::FROM_TOP) ? 0.0f : static_cast<float>(height_);
            link.position.x = width_ / 2.0f + wave_offset;
            link.position.y = start_y + link_pos * dir_mult;
            
            // Alternate rotation: PI/2 (vertical) and 0 (horizontal)
            link.rotation = (i % 2 == 0) ? M_PI / 2.0f : 0.0f;
        }
        
        // Add slight wobble to rotation for realism
        link.rotation += sin(time_ * 3.0f + i * 0.5f) * 0.025f;
    }
}

void OvalChainEffect::spawnTrailLink() {
    // Not used in interlocking chain mode - chain is built at start
}

void OvalChainEffect::updateTrails(float dt) {
    // Not used in interlocking chain mode - chain moves as one unit
}

void OvalChainEffect::detectThreading() {
    // Not used in interlocking chain mode - all links are always threaded
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
    return true;  // All adjacent links are interlocked in the chain
}

float OvalChainEffect::calculateThreadingDepth(const OvalLink& moving, const OvalLink& stationary) {
    return 0.5f;  // Fully interlocked
}

void OvalChainEffect::renderChain(cv::Mat& frame) {
    // For realistic interlocking chains, we need to render in a specific order:
    // Each pair of adjacent links: back of even link -> odd link -> front of even link
    // This creates the illusion that odd links pass through even link holes
    
    if (trail_links_.empty()) return;
    
    // Render links in pairs for proper interlocking appearance
    for (size_t i = 0; i < trail_links_.size(); i++) {
        const OvalLink& link = trail_links_[i];
        
        // Skip if completely off-screen
        float max_dim = std::max(LINK_OUTER_WIDTH, LINK_OUTER_HEIGHT);
        if (link.position.x < -max_dim * 2 || link.position.x > frame.cols + max_dim * 2 ||
            link.position.y < -max_dim * 2 || link.position.y > frame.rows + max_dim * 2) {
            continue;
        }
        
        // Even links (horizontal orientation): draw back half first
        // These are the links that the odd links pass through
        if (i % 2 == 0) {
            drawLinkHalf(frame, link, false);  // Back half
        }
    }
    
    // Now draw all the odd links (the ones that pass through)
    for (size_t i = 1; i < trail_links_.size(); i += 2) {
        const OvalLink& link = trail_links_[i];
        
        float max_dim = std::max(LINK_OUTER_WIDTH, LINK_OUTER_HEIGHT);
        if (link.position.x < -max_dim * 2 || link.position.x > frame.cols + max_dim * 2 ||
            link.position.y < -max_dim * 2 || link.position.y > frame.rows + max_dim * 2) {
            continue;
        }
        
        drawMetallicRing(frame, link);  // Full link
    }
    
    // Finally, draw the front half of even links (on top of odd links)
    for (size_t i = 0; i < trail_links_.size(); i += 2) {
        const OvalLink& link = trail_links_[i];
        
        float max_dim = std::max(LINK_OUTER_WIDTH, LINK_OUTER_HEIGHT);
        if (link.position.x < -max_dim * 2 || link.position.x > frame.cols + max_dim * 2 ||
            link.position.y < -max_dim * 2 || link.position.y > frame.rows + max_dim * 2) {
            continue;
        }
        
        drawLinkHalf(frame, link, true);  // Front half
    }
}

void OvalChainEffect::drawMetallicRing(cv::Mat& frame, const OvalLink& link) {
    // Draw a realistic 3D chain link using a stadium/rounded rectangle shape
    // A chain link looks like two parallel bars connected by rounded ends
    
    cv::Point2f center(link.position.x, link.position.y);
    float angle = link.rotation;
    float brightness = link.brightness;
    
    // Chain link dimensions - realistic proportions
    float outer_length = LINK_OUTER_WIDTH;      // Total length tip to tip
    float outer_width = LINK_OUTER_HEIGHT;      // Width of the link
    float wire_diameter = RING_THICKNESS;       // Thickness of the wire
    
    // Inner hole dimensions (the space where another link passes through)
    float inner_length = outer_length - wire_diameter * 2.2f;
    float inner_width = outer_width - wire_diameter * 2.2f;
    
    // === Metallic color scheme (polished steel) ===
    float base_intensity = brightness * 180.0f;
    cv::Scalar base_metal(
        base_intensity * 0.75f,   // B - cool steel
        base_intensity * 0.80f,   // G
        base_intensity * 0.85f    // R
    );
    
    cv::Scalar highlight(
        std::min(255.0f, brightness * 250.0f * 0.88f),
        std::min(255.0f, brightness * 250.0f * 0.93f),
        std::min(255.0f, brightness * 250.0f)
    );
    
    cv::Scalar shadow(
        base_intensity * 0.35f,
        base_intensity * 0.38f,
        base_intensity * 0.42f
    );
    
    cv::Scalar dark_edge(
        base_intensity * 0.25f,
        base_intensity * 0.28f,
        base_intensity * 0.30f
    );
    
    // Pre-calculate rotation values
    float cos_a = cos(angle);
    float sin_a = sin(angle);
    
    // Helper lambda to rotate a point around center
    auto rotate_point = [&](float x, float y) -> cv::Point {
        float rx = x * cos_a - y * sin_a + center.x;
        float ry = x * sin_a + y * cos_a + center.y;
        return cv::Point(static_cast<int>(rx), static_cast<int>(ry));
    };
    
    // Draw the chain link as a rounded rectangle (stadium shape)
    // using multiple overlapping shapes for a 3D metallic look
    
    // === Outer body of the link ===
    // Draw the main stadium shape
    float half_length = outer_length / 2.0f;
    float half_width = outer_width / 2.0f;
    float end_radius = half_width;  // Rounded ends radius
    
    // Create the main body polygon (rounded rectangle approximation)
    std::vector<cv::Point> outer_poly;
    int segments = 12;
    
    // Right end (semicircle)
    for (int i = 0; i <= segments; i++) {
        float theta = -M_PI/2.0f + M_PI * i / segments;
        float x = (half_length - end_radius) + end_radius * cos(theta);
        float y = end_radius * sin(theta);
        outer_poly.push_back(rotate_point(x, y));
    }
    // Left end (semicircle)
    for (int i = 0; i <= segments; i++) {
        float theta = M_PI/2.0f + M_PI * i / segments;
        float x = -(half_length - end_radius) + end_radius * cos(theta);
        float y = end_radius * sin(theta);
        outer_poly.push_back(rotate_point(x, y));
    }
    
    // Fill the outer shape with base metal color
    cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{outer_poly}, base_metal, cv::LINE_AA);
    
    // === Add 3D shading ===
    // Top highlight strip
    std::vector<cv::Point> highlight_poly;
    float highlight_offset = -half_width * 0.4f;
    float highlight_height = half_width * 0.35f;
    for (int i = 0; i <= segments; i++) {
        float theta = -M_PI/2.0f + M_PI * i / segments;
        float x = (half_length - end_radius) + (end_radius - wire_diameter*0.3f) * cos(theta);
        float y = highlight_offset + highlight_height * (1.0f + sin(theta)) * 0.5f - half_width * 0.3f;
        y = std::max(y, -half_width + wire_diameter * 0.5f);
        highlight_poly.push_back(rotate_point(x, y));
    }
    for (int i = segments; i >= 0; i--) {
        float theta = -M_PI/2.0f + M_PI * i / segments;
        float x = (half_length - end_radius) + (end_radius - wire_diameter*0.6f) * cos(theta);
        float y = highlight_offset - half_width * 0.15f;
        y = std::max(y, -half_width + wire_diameter * 0.3f);
        highlight_poly.push_back(rotate_point(x, y));
    }
    if (highlight_poly.size() > 2) {
        cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{highlight_poly}, highlight, cv::LINE_AA);
    }
    
    // Bottom shadow strip
    std::vector<cv::Point> shadow_poly;
    for (int i = 0; i <= segments; i++) {
        float theta = M_PI/2.0f - M_PI * i / segments;
        float x = (half_length - end_radius) + (end_radius - wire_diameter*0.2f) * cos(theta);
        float y = half_width * 0.3f + (half_width * 0.4f) * (1.0f - cos(theta)) * 0.5f;
        shadow_poly.push_back(rotate_point(x, y));
    }
    for (int i = segments; i >= 0; i--) {
        float theta = M_PI/2.0f - M_PI * i / segments;
        float x = (half_length - end_radius) + (end_radius - wire_diameter*0.5f) * cos(theta);
        float y = half_width * 0.5f;
        shadow_poly.push_back(rotate_point(x, y));
    }
    if (shadow_poly.size() > 2) {
        cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{shadow_poly}, shadow, cv::LINE_AA);
    }
    
    // === Inner hole (where other links pass through) ===
    if (inner_length > 0 && inner_width > 0) {
        float inner_half_length = inner_length / 2.0f;
        float inner_half_width = inner_width / 2.0f;
        float inner_end_radius = inner_half_width;
        
        std::vector<cv::Point> inner_poly;
        // Right end
        for (int i = 0; i <= segments; i++) {
            float theta = -M_PI/2.0f + M_PI * i / segments;
            float x = (inner_half_length - inner_end_radius) + inner_end_radius * cos(theta);
            float y = inner_end_radius * sin(theta);
            inner_poly.push_back(rotate_point(x, y));
        }
        // Left end
        for (int i = 0; i <= segments; i++) {
            float theta = M_PI/2.0f + M_PI * i / segments;
            float x = -(inner_half_length - inner_end_radius) + inner_end_radius * cos(theta);
            float y = inner_end_radius * sin(theta);
            inner_poly.push_back(rotate_point(x, y));
        }
        
        // Fill hole with dark color (background shows through)
        cv::Scalar hole_color(5, 5, 8);
        cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{inner_poly}, hole_color, cv::LINE_AA);
        
        // Inner edge shadow (for depth)
        cv::polylines(frame, std::vector<std::vector<cv::Point>>{inner_poly}, true, dark_edge, 1, cv::LINE_AA);
    }
    
    // === Outer edge ===
    cv::polylines(frame, std::vector<std::vector<cv::Point>>{outer_poly}, true, dark_edge, 1, cv::LINE_AA);
}

void OvalChainEffect::drawLinkHalf(cv::Mat& frame, const OvalLink& link, bool front_half) {
    // Draw either the front or back half of a chain link
    // A chain link has: back curved end -> two straight bars -> front curved end
    // Back half: back end + bars + hole
    // Front half: front curved end only (passes over the threaded link)
    
    cv::Point2f center(link.position.x, link.position.y);
    float angle = link.rotation;
    float brightness = link.brightness;
    
    float outer_length = LINK_OUTER_WIDTH;
    float outer_width = LINK_OUTER_HEIGHT;
    float wire_diameter = RING_THICKNESS;
    float inner_length = outer_length - wire_diameter * 2.2f;
    float inner_width = outer_width - wire_diameter * 2.2f;
    
    // Color scheme
    float base_intensity = brightness * 180.0f;
    cv::Scalar base_metal(
        base_intensity * 0.75f,
        base_intensity * 0.80f,
        base_intensity * 0.85f
    );
    
    cv::Scalar highlight(
        std::min(255.0f, brightness * 250.0f * 0.88f),
        std::min(255.0f, brightness * 250.0f * 0.93f),
        std::min(255.0f, brightness * 250.0f)
    );
    
    cv::Scalar dark_edge(
        base_intensity * 0.25f,
        base_intensity * 0.28f,
        base_intensity * 0.30f
    );
    
    float cos_a = cos(angle);
    float sin_a = sin(angle);
    
    auto rotate_point = [&](float x, float y) -> cv::Point {
        float rx = x * cos_a - y * sin_a + center.x;
        float ry = x * sin_a + y * cos_a + center.y;
        return cv::Point(static_cast<int>(rx), static_cast<int>(ry));
    };
    
    float half_length = outer_length / 2.0f;
    float half_width = outer_width / 2.0f;
    float end_radius = half_width;
    int segments = 12;
    
    float inner_half_length = inner_length / 2.0f;
    float inner_half_width = std::max(1.0f, inner_width / 2.0f);
    float inner_end_radius = inner_half_width;
    
    if (front_half) {
        // Draw the front curved end - the part that passes OVER the threaded link
        // This is just one semicircular end of the stadium shape
        
        std::vector<cv::Point> front_poly;
        
        // Outer curve (right end semicircle)
        for (int i = 0; i <= segments; i++) {
            float theta = -M_PI/2.0f + M_PI * i / segments;
            float x = (half_length - end_radius) + end_radius * cos(theta);
            float y = end_radius * sin(theta);
            front_poly.push_back(rotate_point(x, y));
        }
        
        // Inner curve (closing the ring shape)
        for (int i = segments; i >= 0; i--) {
            float theta = -M_PI/2.0f + M_PI * i / segments;
            float x = (inner_half_length - inner_end_radius) + inner_end_radius * cos(theta);
            float y = inner_end_radius * sin(theta);
            front_poly.push_back(rotate_point(x, y));
        }
        
        if (front_poly.size() > 2) {
            cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{front_poly}, base_metal, cv::LINE_AA);
            cv::polylines(frame, std::vector<std::vector<cv::Point>>{front_poly}, true, dark_edge, 1, cv::LINE_AA);
            
            // Specular highlight on the curved surface
            for (int i = 3; i <= segments - 3; i++) {
                float theta = -M_PI/2.0f + M_PI * i / segments;
                float x = (half_length - end_radius) + (end_radius - wire_diameter * 0.35f) * cos(theta);
                float y = (end_radius - wire_diameter * 0.35f) * sin(theta) - wire_diameter * 0.15f;
                cv::Point p = rotate_point(x, y);
                cv::circle(frame, p, 1, highlight, -1, cv::LINE_AA);
            }
        }
    } else {
        // Draw the back portion: left curved end + both connecting bars + hole
        // This is the part that appears BEHIND the threaded link
        
        // === Draw the two straight side bars ===
        // Top bar (outer)
        std::vector<cv::Point> top_bar;
        top_bar.push_back(rotate_point(-(half_length - end_radius), -half_width));
        top_bar.push_back(rotate_point((half_length - end_radius), -half_width));
        top_bar.push_back(rotate_point((half_length - end_radius), -inner_half_width));
        top_bar.push_back(rotate_point(-(half_length - end_radius), -inner_half_width));
        cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{top_bar}, base_metal, cv::LINE_AA);
        
        // Bottom bar (outer)
        std::vector<cv::Point> bottom_bar;
        bottom_bar.push_back(rotate_point(-(half_length - end_radius), half_width));
        bottom_bar.push_back(rotate_point((half_length - end_radius), half_width));
        bottom_bar.push_back(rotate_point((half_length - end_radius), inner_half_width));
        bottom_bar.push_back(rotate_point(-(half_length - end_radius), inner_half_width));
        cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{bottom_bar}, base_metal, cv::LINE_AA);
        
        // === Draw the back curved end (left semicircle) ===
        std::vector<cv::Point> back_end;
        
        // Outer curve
        for (int i = 0; i <= segments; i++) {
            float theta = M_PI/2.0f + M_PI * i / segments;
            float x = -(half_length - end_radius) + end_radius * cos(theta);
            float y = end_radius * sin(theta);
            back_end.push_back(rotate_point(x, y));
        }
        
        // Inner curve
        for (int i = segments; i >= 0; i--) {
            float theta = M_PI/2.0f + M_PI * i / segments;
            float x = -(inner_half_length - inner_end_radius) + inner_end_radius * cos(theta);
            float y = inner_end_radius * sin(theta);
            back_end.push_back(rotate_point(x, y));
        }
        
        if (back_end.size() > 2) {
            cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{back_end}, base_metal, cv::LINE_AA);
        }
        
        // === Draw the center hole ===
        std::vector<cv::Point> hole_poly;
        // Right inner curve
        for (int i = 0; i <= segments; i++) {
            float theta = -M_PI/2.0f + M_PI * i / segments;
            float x = (inner_half_length - inner_end_radius) + inner_end_radius * cos(theta);
            float y = inner_end_radius * sin(theta);
            hole_poly.push_back(rotate_point(x, y));
        }
        // Left inner curve
        for (int i = 0; i <= segments; i++) {
            float theta = M_PI/2.0f + M_PI * i / segments;
            float x = -(inner_half_length - inner_end_radius) + inner_end_radius * cos(theta);
            float y = inner_end_radius * sin(theta);
            hole_poly.push_back(rotate_point(x, y));
        }
        
        cv::Scalar hole_color(5, 5, 8);
        cv::fillPoly(frame, std::vector<std::vector<cv::Point>>{hole_poly}, hole_color, cv::LINE_AA);
        
        // === Draw edges for definition ===
        // Outer edges of bars
        cv::line(frame, rotate_point(-(half_length - end_radius), -half_width),
                 rotate_point((half_length - end_radius), -half_width), dark_edge, 1, cv::LINE_AA);
        cv::line(frame, rotate_point(-(half_length - end_radius), half_width),
                 rotate_point((half_length - end_radius), half_width), dark_edge, 1, cv::LINE_AA);
        
        // Back end outline
        cv::polylines(frame, std::vector<std::vector<cv::Point>>{back_end}, true, dark_edge, 1, cv::LINE_AA);
        
        // Inner hole edge
        cv::polylines(frame, std::vector<std::vector<cv::Point>>{hole_poly}, true, dark_edge, 1, cv::LINE_AA);
        
        // Highlight on top bar
        cv::line(frame, rotate_point(-(half_length - end_radius), -half_width + wire_diameter * 0.25f),
                 rotate_point((half_length - end_radius), -half_width + wire_diameter * 0.25f), 
                 highlight, 1, cv::LINE_AA);
    }
}

void OvalChainEffect::drawOvalLink(cv::Mat& frame, const OvalLink& link) {
    drawMetallicRing(frame, link);
}
