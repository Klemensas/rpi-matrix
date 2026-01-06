#include "components/software_matrix_display.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

SoftwareMatrixDisplay::SoftwareMatrixDisplay(int rows, int cols, int chain_length, int parallel,
                                             const std::string& window_name)
    : rows_(rows),
      cols_(cols),
      chain_length_(chain_length),
      parallel_(parallel),
      window_name_(window_name) {
    matrix_w_ = cols_ * chain_length_;
    matrix_h_ = rows_ * parallel_;

    matrix_bgr_.create(matrix_h_, matrix_w_, CV_8UC3);

    cv::namedWindow(window_name_, cv::WINDOW_NORMAL);
    cv::resizeWindow(window_name_, 800, 800);
}

int SoftwareMatrixDisplay::getWidth() const { return matrix_w_; }
int SoftwareMatrixDisplay::getHeight() const { return matrix_h_; }

int SoftwareMatrixDisplay::displayFrame(const cv::Mat& bgr, int delay_ms) {
    if (bgr.empty()) return cv::waitKey(delay_ms);

    // Downscale to matrix resolution (what hardware would show)
    cv::resize(bgr, matrix_bgr_, cv::Size(matrix_w_, matrix_h_), 0, 0, cv::INTER_AREA);

    // Upscale for viewing (nearest neighbor so it looks pixelated like LEDs)
    cv::resize(matrix_bgr_, preview_bgr_, cv::Size(matrix_w_ * 10, matrix_h_ * 10), 0, 0, cv::INTER_NEAREST);

    cv::imshow(window_name_, preview_bgr_);
    return cv::waitKey(delay_ms);
}
