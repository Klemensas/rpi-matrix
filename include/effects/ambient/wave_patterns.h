#ifndef WAVE_PATTERNS_EFFECT_H
#define WAVE_PATTERNS_EFFECT_H

#include <opencv2/core.hpp>

class WavePatternsEffect {
public:
    explicit WavePatternsEffect(int width, int height);

    void reset();
    void process(cv::Mat& out_bgr, int target_width = -1, int target_height = -1);

private:
    cv::Scalar hsvToBgr(float h, float s, float v);

    int width_;
    int height_;

    // Wave patterns state
    float wave_time_;
    float wave_phase_;
};

#endif // WAVE_PATTERNS_EFFECT_H
