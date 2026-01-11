#ifndef APP_CORE_H
#define APP_CORE_H

#include <atomic>
#include <cstdint>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/video/background_segm.hpp>

// Platform-agnostic core processing (OpenCV-only).
// All frames are expected to be CV_8UC3 in **BGR** order.
class AppCore {
public:
    explicit AppCore(int width, int height, int num_panels = 1);

    void setDisplayMode(int mode);
    int displayMode() const;
    
    // Multi-panel mode: independent state that overrides display mode
    void setMultiPanelEnabled(bool enabled);
    bool isMultiPanelEnabled() const { return multi_panel_enabled_; }
    
    // Multi-panel mode: set effect for individual panel
    void setPanelEffect(int panel_index, int effect);
    int getPanelEffect(int panel_index) const;
    int getNumPanels() const { return num_panels_; }

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
    void processMultiPanel(const cv::Mat& in_bgr, cv::Mat& out_bgr);
    
    void processPanelRegion(const cv::Mat& in_region, cv::Mat& out_region, int effect, int panel_index);
    void ensurePanelResourcesInitialized();

    std::atomic<int> display_mode_{1};
    std::atomic<bool> multi_panel_enabled_{false};  // Multi-panel mode state
    
    static constexpr int MAX_PANELS = 8;  // Support up to 8 chained panels
    std::atomic<int> panel_effects_[MAX_PANELS];  // Effects for each panel

    int width_;
    int height_;
    int num_panels_;

    cv::Ptr<cv::BackgroundSubtractor> background_subtractor_;
    cv::Mat silhouette_frame_; // persistent buffer for trails/energy
    float trail_alpha_ = 0.7f;
    float energy_decay_ = 0.97f;
    
    // Per-panel resources for multi-panel mode (lazy initialized)
    bool panel_resources_initialized_ = false;
    std::vector<cv::Ptr<cv::BackgroundSubtractor>> panel_bg_subtractors_;
    std::vector<cv::Mat> panel_silhouette_frames_;
};

#endif // APP_CORE_H
