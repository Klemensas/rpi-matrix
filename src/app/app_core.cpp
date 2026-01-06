#include "app/app_core.h"

#include <opencv2/imgproc.hpp>

AppCore::AppCore(int width, int height)
    : width_(width),
      height_(height),
      background_subtractor_(cv::createBackgroundSubtractorMOG2(500, 16, true)) {
    silhouette_frame_ = cv::Mat::zeros(height_, width_, CV_8UC3);
}

void AppCore::setDisplayMode(int mode) {
    display_mode_.store(mode);
}

int AppCore::displayMode() const {
    return display_mode_.load();
}

void AppCore::ensureSize(int w, int h) {
    if (w == width_ && h == height_ && !silhouette_frame_.empty()) return;
    width_ = w;
    height_ = h;
    silhouette_frame_ = cv::Mat::zeros(height_, width_, CV_8UC3);
}

void AppCore::processFrame(const cv::Mat& in_bgr, cv::Mat& out_bgr) {
    if (in_bgr.empty()) return;
    ensureSize(in_bgr.cols, in_bgr.rows);

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
