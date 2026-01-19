#ifndef MANDELBROT_ROOT_VEINS_EFFECT_H
#define MANDELBROT_ROOT_VEINS_EFFECT_H

#include <vector>
#include <opencv2/core.hpp>

// Represents a single vein segment in the fractal network
struct VeinSegment {
    cv::Point2f start;
    cv::Point2f end;
    float age;           // Time since creation (for growth animation)
    int generation;      // Branch depth (0 = root, higher = child branches)
    bool is_wilting;     // True if segment is dying due to cap overflow
    float wilt_progress; // 0.0 = alive, 1.0 = fully wilted
    float phase;         // Random phase offset for pulsation
    float direction;     // Angle in radians for growth direction
    bool is_tip;         // True if this is an actively growing tip
};

class MandelbrotRootVeinsEffect {
public:
    explicit MandelbrotRootVeinsEffect(int width, int height);

    void reset();
    void process(cv::Mat& out_bgr, int target_width = -1, int target_height = -1);

private:
    // Growth and branching
    void initializeRootVeins();
    void growVeins(float dt);
    void checkIntersections();
    void updateWilting(float dt);
    void spawnBranch(const cv::Point2f& origin, float base_direction, int parent_generation);
    
    // Mandelbrot-inspired direction calculation
    float getMandelbrotDirection(float x, float y, float base_angle);
    
    // Rendering
    void renderVeins(cv::Mat& frame);
    float getSegmentBrightness(const VeinSegment& seg);
    
    // Coordinate transforms
    cv::Point2f applyZoomRotation(const cv::Point2f& p);
    
    // Utility
    bool segmentsIntersect(const cv::Point2f& p1, const cv::Point2f& p2,
                           const cv::Point2f& p3, const cv::Point2f& p4,
                           cv::Point2f& intersection);

    int width_;
    int height_;

    // Vein state
    std::vector<VeinSegment> segments_;
    float time_;
    float zoom_;
    float rotation_;
    
    // Processing buffer (half resolution)
    cv::Mat proc_frame_;
    cv::Mat glow_frame_;
    
    // Configuration
    static constexpr int MAX_SEGMENTS = 800;         // More segments for denser veins
    static constexpr float GROWTH_SPEED = 3.0f;      // Pixels per frame at base
    static constexpr float WILT_SPEED = 0.02f;       // Wilt progress per frame
    static constexpr float ZOOM_RATE = 0.0002f;      // Zoom multiplier per frame
    static constexpr float ROTATION_RATE = 0.002f;   // Radians per frame
    static constexpr float BRANCH_ANGLE_SPREAD = 0.45f; // Tighter angles for realistic veins
    static constexpr int MAX_GENERATION = 8;         // More branch depth for fine detail
};

#endif // MANDELBROT_ROOT_VEINS_EFFECT_H
