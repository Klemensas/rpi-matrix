#ifndef OVAL_CHAIN_EFFECT_H
#define OVAL_CHAIN_EFFECT_H

#include <vector>
#include <opencv2/core.hpp>

// Direction the chain travels from
enum class ChainDirection {
    FROM_LEFT,
    FROM_RIGHT,
    FROM_TOP,
    FROM_BOTTOM
};

// Represents a single oval link in the chain (torus/ring shape)
struct OvalLink {
    int id;                   // Unique identifier for this link
    cv::Point2f position;     // Center position
    float rotation;           // Rotation angle in radians
    float brightness;         // 0.0 to 1.0 (can exceed 1.0 for highlight boost)
    int z_order;              // For layering (higher = on top)
    float age;                // Time since creation (for trail fading)
    bool is_active;           // True if this is the current moving link (not trail)
    
    // Threading state
    bool is_threading;        // Currently threading through another link
    int threading_with_id;    // ID of link being threaded through (-1 if none)
    float threading_depth;    // 0.0 = entering, 0.5 = middle, 1.0 = exiting
    
    // Motion state
    cv::Point2f velocity;     // Current movement vector
    float oscillation_phase;  // Phase offset for natural wave motion
};

class OvalChainEffect {
public:
    explicit OvalChainEffect(int width, int height);

    void process(cv::Mat& out_bgr, int target_width = -1, int target_height = -1);

private:
    // Core update methods
    void updateChain(float dt);
    void spawnTrailLink();
    void updateTrails(float dt);
    void detectThreading();
    void startNewChain();
    
    // Rendering methods
    void renderChain(cv::Mat& frame);
    void drawOvalLink(cv::Mat& frame, const OvalLink& link);
    void drawMetallicRing(cv::Mat& frame, const OvalLink& link);
    
    // Threading detection helpers
    bool isPointInEllipseHole(const cv::Point2f& point, const OvalLink& ring);
    bool checkThreadingCondition(const OvalLink& moving, const OvalLink& stationary);
    float calculateThreadingDepth(const OvalLink& moving, const OvalLink& stationary);

    int width_;
    int height_;
    float time_;
    int next_link_id_;
    
    // The active chain link (head) and its trail
    OvalLink active_link_;
    std::vector<OvalLink> trail_links_;
    
    // Timing for trail spawning
    float last_trail_spawn_time_;
    float cycle_start_time_;
    
    // Current chain direction
    ChainDirection current_direction_;

    // Configuration - Motion
    static constexpr float TRAVERSE_TIME = 1.0f;          // Seconds to cross screen
    static constexpr float OSCILLATION_AMPLITUDE = 10.0f; // Oscillation pixels
    static constexpr float OSCILLATION_FREQUENCY = 1.2f;  // Oscillations per traverse
    static constexpr float ROTATION_SPEED = 0.4f;         // Radians per second rotation
    
    // Configuration - Trail
    static constexpr float TRAIL_SPAWN_INTERVAL = 0.15f;  // Seconds between trail links (3x slower)
    static constexpr float TRAIL_FADE_TIME = 1.8f;        // Seconds for trail to fully fade
    static constexpr int MAX_TRAIL_LINKS = 25;            // Maximum trail length
    
    // Configuration - Link appearance (more elongated chain-like)
    static constexpr float LINK_OUTER_WIDTH = 38.0f;      // Outer ellipse width (elongated)
    static constexpr float LINK_OUTER_HEIGHT = 14.0f;     // Outer ellipse height (thin)
    static constexpr float HOLE_RATIO = 0.50f;            // Inner hole size (0.0-1.0)
    static constexpr float RING_THICKNESS = 5.0f;         // Visual thickness of ring
    
    // Configuration - Visual effects
    static constexpr float THREADING_BRIGHTNESS_BOOST = 0.4f;  // Extra brightness when threading
    static constexpr float BASE_BRIGHTNESS = 0.85f;            // Normal link brightness
    static constexpr float MIN_TRAIL_BRIGHTNESS = 0.15f;       // Minimum before removal
};

#endif // OVAL_CHAIN_EFFECT_H
