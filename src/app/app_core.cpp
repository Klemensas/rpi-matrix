#include "app/app_core.h"

#include <opencv2/imgproc.hpp>

AppCore::AppCore(int width, int height, int num_panels)
    : width_(width),
      height_(height),
      num_panels_(num_panels),
      background_subtractor_(cv::createBackgroundSubtractorMOG2(500, 16, true)) {
    silhouette_frame_ = cv::Mat::zeros(height_, width_, CV_8UC3);
    
    // Initialize panel effects to default (resources created lazily)
    for (int i = 0; i < num_panels_; i++) {
        panel_effects_[i].store(1);  // Default to pass-through
    }
}

void AppCore::setDisplayMode(int mode) {
    display_mode_.store(mode);
}

int AppCore::displayMode() const {
    return display_mode_.load();
}

void AppCore::setPanelEffect(int panel_index, int effect) {
    if (panel_index >= 0 && panel_index < num_panels_) {
        panel_effects_[panel_index].store(effect);
    }
}

int AppCore::getPanelEffect(int panel_index) const {
    if (panel_index >= 0 && panel_index < num_panels_) {
        return panel_effects_[panel_index].load();
    }
    return 1; // default
}

void AppCore::ensureSize(int w, int h) {
    if (w == width_ && h == height_ && !silhouette_frame_.empty()) return;
    width_ = w;
    height_ = h;
    silhouette_frame_ = cv::Mat::zeros(height_, width_, CV_8UC3);
}

void AppCore::setMultiPanelEnabled(bool enabled) {
    multi_panel_enabled_.store(enabled);
}

void AppCore::processFrame(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    if (in_bgr.empty()) return;
    ensureSize(in_bgr.cols, in_bgr.rows);

    // Multi-panel mode overrides display mode
    if (multi_panel_enabled_.load()) {
        processMultiPanel(in_bgr, out_bgr);
        return;
    }

    // Normal display modes
    switch (display_mode_.load()) {
        case 1:
            processPassThrough(in_bgr, out_bgr);
            break;
        case 2:
            processFilledSilhouette(in_bgr, out_bgr);
            break;
        case 3:
            processOutline(in_bgr, out_bgr);
            break;
        case 4:
            processMotionTrails(in_bgr, out_bgr);
            break;
        case 5:
            processEnergyMotion(in_bgr, out_bgr);
            break;
        default:
            processPassThrough(in_bgr, out_bgr);
            break;
    }
}

void AppCore::processPassThrough(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    out_bgr = in_bgr; // shallow copy ok; display should not mutate
}

static void findPersonContours(const cv::Mat& fg_mask,
                               std::vector<std::vector<cv::Point>>& contours_out,
                               int min_contour_area) {
    std::vector<cv::Vec4i> hierarchy;
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(fg_mask, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    contours_out.clear();
    contours_out.reserve(contours.size());
    for (const auto& c : contours) {
        if (cv::contourArea(c) > min_contour_area) contours_out.push_back(c);
    }
}

void AppCore::processFilledSilhouette(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);

    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1000);

    out_bgr = cv::Mat::zeros(in_bgr.rows, in_bgr.cols, CV_8UC3);
    for (const auto& c : contours) {
        cv::drawContours(out_bgr, std::vector<std::vector<cv::Point>>{c}, -1,
                         cv::Scalar(255, 255, 255), cv::FILLED);
    }
}

void AppCore::processOutline(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);

    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1000);

    out_bgr = cv::Mat::zeros(in_bgr.rows, in_bgr.cols, CV_8UC3);
    for (const auto& c : contours) {
        cv::drawContours(out_bgr, std::vector<std::vector<cv::Point>>{c}, -1,
                         cv::Scalar(255, 255, 255), /*thickness=*/2);
    }
}

void AppCore::processMotionTrails(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);

    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1000);

    silhouette_frame_ *= trail_alpha_;
    for (const auto& c : contours) {
        cv::drawContours(silhouette_frame_, std::vector<std::vector<cv::Point>>{c}, -1,
                         cv::Scalar(255, 255, 255), cv::FILLED);
    }
    out_bgr = silhouette_frame_;
}

void AppCore::processEnergyMotion(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    cv::Mat fg_mask;
    background_subtractor_->apply(in_bgr, fg_mask);

    std::vector<std::vector<cv::Point>> contours;
    findPersonContours(fg_mask, contours, /*min_contour_area=*/1000);

    // Fast uniform decay + overwrite with bright new silhouettes
    silhouette_frame_ *= 0.92f;
    for (const auto& c : contours) {
        cv::drawContours(silhouette_frame_, std::vector<std::vector<cv::Point>>{c}, -1,
                         cv::Scalar(255, 255, 255), cv::FILLED);
    }
    out_bgr = silhouette_frame_;
}

void AppCore::ensurePanelResourcesInitialized() {
    if (!panel_resources_initialized_) {
        panel_bg_subtractors_.resize(num_panels_);
        panel_silhouette_frames_.resize(num_panels_);
        
        for (int i = 0; i < num_panels_; i++) {
            panel_bg_subtractors_[i] = cv::createBackgroundSubtractorMOG2(500, 16, true);
            panel_silhouette_frames_[i] = cv::Mat::zeros(height_, width_ / num_panels_, CV_8UC3);
        }
        panel_resources_initialized_ = true;
    }
}

void AppCore::processMultiPanel(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    // Lazy initialization of per-panel resources
    ensurePanelResourcesInitialized();
    
    // Split input into equal horizontal regions (for chained panels)
    int panel_width = in_bgr.cols / num_panels_;
    
    out_bgr = cv::Mat(in_bgr.rows, in_bgr.cols, CV_8UC3);
    
    for (int i = 0; i < num_panels_; i++) {
        int x_start = i * panel_width;
        int x_end = (i == num_panels_ - 1) ? in_bgr.cols : (i + 1) * panel_width;  // Last panel gets remainder
        
        cv::Rect panel_roi(x_start, 0, x_end - x_start, in_bgr.rows);
        cv::Mat in_region = in_bgr(panel_roi);
        cv::Mat out_region = out_bgr(panel_roi);
        
        int effect = panel_effects_[i].load();
        processPanelRegion(in_region, out_region, effect, i);
    }
}

void AppCore::processPanelRegion(const cv::Mat& in_region, cv::Mat& out_region, int effect, int panel_index) {
    // Ensure per-panel buffers are correctly sized
    int w = in_region.cols;
    int h = in_region.rows;
    if (panel_silhouette_frames_[panel_index].cols != w || 
        panel_silhouette_frames_[panel_index].rows != h) {
        panel_silhouette_frames_[panel_index] = cv::Mat::zeros(h, w, CV_8UC3);
    }
    
    cv::Mat temp_output;
    
    // Apply the specified effect to this region
    switch (effect) {
        case 1:
            // Pass-through
            in_region.copyTo(out_region);
            break;
        case 2:
            // Filled silhouette
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                temp_output = cv::Mat::zeros(h, w, CV_8UC3);
                for (const auto& c : contours) {
                    cv::drawContours(temp_output, std::vector<std::vector<cv::Point>>{c}, -1,
                                   cv::Scalar(255, 255, 255), cv::FILLED);
                }
                temp_output.copyTo(out_region);
            }
            break;
        case 3:
            // Outline
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                temp_output = cv::Mat::zeros(h, w, CV_8UC3);
                for (const auto& c : contours) {
                    cv::drawContours(temp_output, std::vector<std::vector<cv::Point>>{c}, -1,
                                   cv::Scalar(255, 255, 255), /*thickness=*/2);
                }
                temp_output.copyTo(out_region);
            }
            break;
        case 4:
            // Motion trails
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                panel_silhouette_frames_[panel_index] *= 0.7f;
                for (const auto& c : contours) {
                    cv::drawContours(panel_silhouette_frames_[panel_index], 
                                   std::vector<std::vector<cv::Point>>{c}, -1,
                                   cv::Scalar(255, 255, 255), cv::FILLED);
                }
                panel_silhouette_frames_[panel_index].copyTo(out_region);
            }
            break;
        case 5:
            // Energy motion
            {
                cv::Mat fg_mask;
                panel_bg_subtractors_[panel_index]->apply(in_region, fg_mask);
                
                std::vector<std::vector<cv::Point>> contours;
                findPersonContours(fg_mask, contours, /*min_contour_area=*/500);
                
                panel_silhouette_frames_[panel_index] *= 0.92f;
                for (const auto& c : contours) {
                    cv::drawContours(panel_silhouette_frames_[panel_index], 
                                   std::vector<std::vector<cv::Point>>{c}, -1,
                                   cv::Scalar(255, 255, 255), cv::FILLED);
                }
                panel_silhouette_frames_[panel_index].copyTo(out_region);
            }
            break;
        default:
            in_region.copyTo(out_region);
            break;
    }
}
