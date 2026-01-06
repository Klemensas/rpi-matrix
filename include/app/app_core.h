#ifndef APP_CORE_H
#define APP_CORE_H

#include <atomic>
#include <cstdint>
#include <opencv2/core.hpp>
#include <opencv2/video/background_segm.hpp>

// Platform-agnostic core processing (OpenCV-only).
// All frames are expected to be CV_8UC3 in **BGR** order.
class AppCore {
public:
    explicit AppCore(int width, int height);

    void setDisplayMode(int mode);
    int displayMode() const;

    // Process an input frame into an output frame.
    // - in_bgr: CV_8UC3 BGR image (size can differ; core will adapt internal buffers)
    // - out_bgr: written as CV_8UC3 BGR image, same size as in_bgr
    void processFrame(const cv::Mat& in_bgr, cv::Mat& out_bgr);

private:
    void ensureSize(int w, int h);

    void processPassThrough(const cv::Mat& in_bgr, cv::Mat& out_bgr);
    void processFilledSilhouette(const cv::Mat& in_bgr, cv::Mat& out_bgr);
    void processOutline(const cv::Mat& in_bgr, cv::Mat& out_bgr);
    void processMotionTrails(const cv::Mat& in_bgr, cv::Mat& out_bgr);
    void processEnergyMotion(const cv::Mat& in_bgr, cv::Mat& out_bgr);

    std::atomic<int> display_mode_{1};

    int width_;
    int height_;

    cv::Ptr<cv::BackgroundSubtractor> background_subtractor_;
    cv::Mat silhouette_frame_; // persistent buffer for trails/energy
    float trail_alpha_ = 0.7f;
    float energy_decay_ = 0.97f;
};

#endif // APP_CORE_H
