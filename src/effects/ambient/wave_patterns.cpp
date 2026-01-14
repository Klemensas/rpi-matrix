#include "effects/ambient/wave_patterns.h"
#include <cmath>
#include <opencv2/imgproc.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

WavePatternsEffect::WavePatternsEffect(int width, int height)
    : width_(width), height_(height) {
    reset();
}

void WavePatternsEffect::reset() {
    wave_time_ = 0.0f;
    wave_phase_ = 0.0f;
}

// Effect 8: Wave Patterns (Ambient System Mode)
void WavePatternsEffect::process(cv::Mat& out_bgr) {
    out_bgr = cv::Mat::zeros(height_, width_, CV_8UC3);

    wave_time_ += 0.05f;
    wave_phase_ += 0.02f;

    // Optimize: Process at lower resolution then upscale for better performance
    // Process at half resolution for 4x speedup
    int proc_w = width_ / 2;
    int proc_h = height_ / 2;
    if (proc_w < 1) proc_w = 1;
    if (proc_h < 1) proc_h = 1;

    cv::Mat proc_frame(proc_h, proc_w, CV_8UC3);

    // Create interference pattern with multiple waves at reduced resolution
    for (int y = 0; y < proc_h; y++) {
        for (int x = 0; x < proc_w; x++) {
            // Scale coordinates back to original size for wave calculations
            float fx = (x * 2.0f) * 0.1f;
            float fy = (y * 2.0f) * 0.1f;

            // Multiple sine waves for interference
            float wave1 = std::sin(fx + wave_time_);
            float wave2 = std::sin(fy + wave_time_ * 1.3f);
            float wave3 = std::sin((fx + fy) * 0.07f + wave_phase_);

            float combined = (wave1 + wave2 + wave3) / 3.0f;

            // Map to color (hue based on position, brightness based on wave)
            float hue = fmod((fx + fy) * 10.0f + wave_time_ * 20.0f, 360.0f);
            float brightness = (combined + 1.0f) * 0.5f;  // Normalize to 0-1
            cv::Scalar color = hsvToBgr(hue, 1.0f, brightness);

            proc_frame.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<uint8_t>(color[0]),
                static_cast<uint8_t>(color[1]),
                static_cast<uint8_t>(color[2])
            );
        }
    }

    // Upscale to full resolution
    cv::resize(proc_frame, out_bgr, cv::Size(width_, height_), 0, 0, cv::INTER_LINEAR);
}

// Helper function to convert HSV to BGR
cv::Scalar WavePatternsEffect::hsvToBgr(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - std::abs(fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h < 60) {
        r = c; g = x; b = 0;
    } else if (h < 120) {
        r = x; g = c; b = 0;
    } else if (h < 180) {
        r = 0; g = c; b = x;
    } else if (h < 240) {
        r = 0; g = x; b = c;
    } else if (h < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }

    return cv::Scalar((b + m) * 255, (g + m) * 255, (r + m) * 255);
}
