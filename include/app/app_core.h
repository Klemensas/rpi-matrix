#ifndef APP_CORE_H
#define APP_CORE_H

#include <atomic>
#include <cstdint>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/video/background_segm.hpp>

// Panel layout modes for multi-panel display
enum class PanelMode {
    EXTEND,  // Image extends/spans across panels (split horizontally)
    REPEAT   // Same image repeated on each panel (with different effects)
};

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
    
    // Multi-panel layout mode
    void setPanelMode(PanelMode mode);
    PanelMode getPanelMode() const;
    
    // Multi-panel mode: set effect for individual panel
    void setPanelEffect(int panel_index, int effect);
    int getPanelEffect(int panel_index) const;
    int getNumPanels() const { return num_panels_; }
    
    // Auto-cycling controls
    void toggleAutoCycling();
    bool isAutoCycling() const { return auto_cycling_enabled_; }

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
    void processRainbowTrails(const cv::Mat& in_bgr, cv::Mat& out_bgr);
    void processDoubleExposure(const cv::Mat& in_bgr, cv::Mat& out_bgr);
    void processProceduralShapes(cv::Mat& out_bgr);
    void processWavePatterns(cv::Mat& out_bgr);
    void processGeometricAbstraction(const cv::Mat& in_bgr, cv::Mat& out_bgr);
    void processMultiPanel(const cv::Mat& in_bgr, cv::Mat& out_bgr);
    
    // Procedural shapes helpers
    cv::Scalar hsvToBgr(float h, float s, float v);
    void drawMorphingShape(cv::Mat& img, int cx, int cy, int radius, 
                          int shape_type, float morph_progress, cv::Scalar color,
                          float fill_mode);
    std::vector<cv::Point> getShapePoints(int shape_type, int cx, int cy, int radius);
    
    void processPanelRegion(const cv::Mat& in_region, cv::Mat& out_region, int effect, int panel_index);
    void ensurePanelResourcesInitialized();
    
    // Auto mode cycling (internal)
    void updateAutoCycling();
    int getRandomCycleInterval();
 
    std::atomic<int> display_mode_{1};
    std::atomic<bool> multi_panel_enabled_{false};  // Multi-panel mode state
    std::atomic<int> panel_mode_{0};  // 0=EXTEND, 1=REPEAT (atomic for thread safety)
    
    static constexpr int MAX_PANELS = 8;  // Support up to 8 chained panels
    std::atomic<int> panel_effects_[MAX_PANELS];  // Effects for each panel

    int width_;
    int height_;
    int num_panels_;

    cv::Ptr<cv::BackgroundSubtractor> background_subtractor_;
    cv::Mat silhouette_frame_; // persistent buffer for trails/energy
    cv::Mat trail_age_buffer_;  // Float buffer tracking age of each trail pixel (for rainbow)
    float trail_alpha_ = 0.7f;
    float energy_decay_ = 0.97f;
    
    // Double exposure buffers (time-based with randomization)
    std::vector<cv::Mat> frame_history_;   // Ring buffer of past frames
    int frame_history_index_ = 0;
    int frame_counter_ = 0;  // Count frames to trigger random time offset changes
    int current_time_offset_ = 30;  // Current random offset (in frames)
    static constexpr int MAX_FRAME_HISTORY = 90;  // Store ~3 seconds at 30fps
    static constexpr int MIN_TIME_OFFSET = 15;    // Min 0.5 sec at 30fps
    static constexpr int MAX_TIME_OFFSET = 75;    // Max 2.5 sec at 30fps
    
    // Per-panel resources for multi-panel mode (lazy initialized)
    bool panel_resources_initialized_ = false;
    std::vector<cv::Ptr<cv::BackgroundSubtractor>> panel_bg_subtractors_;
    std::vector<cv::Mat> panel_silhouette_frames_;
    
    // Per-panel Mode 7 (Double Exposure) resources
    std::vector<std::vector<cv::Mat>> panel_frame_history_;  // Frame history for each panel
    std::vector<int> panel_frame_history_index_;             // Circular buffer index per panel
    std::vector<int> panel_frame_counter_;                   // Frame counter per panel
    std::vector<int> panel_time_offset_;                     // Time offset per panel
    
    // Helper to process Mode 7 with specific state
    void processDoubleExposureWithState(const cv::Mat& in_bgr, cv::Mat& out_bgr,
                                        std::vector<cv::Mat>& history,
                                        int& history_index,
                                        int& frame_counter,
                                        int& time_offset,
                                        cv::Ptr<cv::BackgroundSubtractor> bg_subtractor);
    
    // Auto-cycling state
    bool auto_cycling_enabled_ = true;
    int cycle_frame_counter_ = 0;
    int frames_until_next_mode_ = 0;
    int transition_frames_remaining_ = 0;
    int previous_mode_ = 1;
    float transition_alpha_ = 0.0f;
    cv::Mat previous_frame_output_;
    
    static constexpr int MIN_CYCLE_SECONDS = 3;
    static constexpr int MAX_CYCLE_SECONDS = 7;
    static constexpr int TRANSITION_FRAMES = 30;  // 1 second transition at 30fps
    
    // Procedural shapes state
    int procedural_frame_counter_ = 0;
    float procedural_time_ = 0.0f;
    int current_shape_type_ = 0;  // 0=circle, 1=triangle, 2=square, 3=hexagon, 4=star
    float shape_morph_progress_ = 0.0f;
    float hue_shift_ = 0.0f;
    float fill_mode_progress_ = 0.0f;  // 0.0 = outline only, 1.0 = filled
    float color_morph_progress_ = 0.0f;  // For color morphing
    
    // Wave patterns state
    float wave_time_ = 0.0f;
    float wave_phase_ = 0.0f;
};

#endif // APP_CORE_H
