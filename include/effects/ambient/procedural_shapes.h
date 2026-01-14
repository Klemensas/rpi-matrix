#ifndef PROCEDURAL_SHAPES_EFFECT_H
#define PROCEDURAL_SHAPES_EFFECT_H

#include <vector>
#include <opencv2/core.hpp>

class ProceduralShapesEffect {
public:
    explicit ProceduralShapesEffect(int width, int height);

    void reset();
    void process(cv::Mat& out_bgr);

private:
    // Helper functions
    cv::Scalar hsvToBgr(float h, float s, float v);
    void drawMorphingShape(cv::Mat& img, int cx, int cy, int radius,
                          int shape_type, float morph_progress, cv::Scalar color,
                          float fill_mode);
    std::vector<cv::Point> getShapePoints(int shape_type, int cx, int cy, int radius);

    int width_;
    int height_;

    // Procedural shapes state
    int procedural_frame_counter_;
    float procedural_time_;
    int current_shape_type_;  // 0=circle, 1=triangle, 2=square, 3=hexagon, 4=star
    float shape_morph_progress_;
    float hue_shift_;
    float fill_mode_progress_;  // 0.0 = outline only, 1.0 = filled
    float color_morph_progress_;  // For color morphing
};

#endif // PROCEDURAL_SHAPES_EFFECT_H
