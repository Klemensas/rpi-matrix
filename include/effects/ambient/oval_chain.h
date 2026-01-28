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
    void drawLinkHalf(cv::Mat& frame, const OvalLink& link, bool front_half);
    void buildInterlockingChain();
    
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
    static constexpr float TRAVERSE_TIME = 4.0f;          // Seconds to cross screen (slower for effect)
    static constexpr float OSCILLATION_AMPLITUDE = 6.0f;  // Wave motion amplitude (pixels)
    static constexpr float OSCILLATION_FREQUENCY = 1.2f;  // Oscillations per traverse
    static constexpr float ROTATION_SPEED = 0.4f;         // Radians per second rotation
    
    // Configuration - Chain
    static constexpr float TRAIL_SPAWN_INTERVAL = 0.15f;  // (unused in interlocking mode)
    static constexpr float TRAIL_FADE_TIME = 1.8f;        // (unused in interlocking mode)
    static constexpr int MAX_TRAIL_LINKS = 40;            // Maximum links in chain
    
    // Configuration - Link appearance (realistic chain link proportions)
    static constexpr float LINK_OUTER_WIDTH = 28.0f;      // Length of link (tip to tip)
    static constexpr float LINK_OUTER_HEIGHT = 12.0f;     // Width of link
    static constexpr float HOLE_RATIO = 0.55f;            // Inner hole size ratio
    static constexpr float RING_THICKNESS = 4.0f;         // Wire diameter of the link
    
    // Configuration - Visual effects
    static constexpr float THREADING_BRIGHTNESS_BOOST = 0.4f;  // Extra brightness when threading
    static constexpr float BASE_BRIGHTNESS = 0.85f;            // Normal link brightness
    static constexpr float MIN_TRAIL_BRIGHTNESS = 0.15f;       // Minimum before removal
};

#endif // OVAL_CHAIN_EFFECT_H
