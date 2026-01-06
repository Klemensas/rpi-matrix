#ifndef SOFTWARE_MATRIX_DISPLAY_H
#define SOFTWARE_MATRIX_DISPLAY_H

#include <string>
#include <opencv2/core.hpp>

// Desktop-only "software matrix" preview.
// Mimics the physical matrix by downscaling to (matrix_width x matrix_height) and then upscaling.
// All frames are expected to be CV_8UC3 in **BGR** order.
class SoftwareMatrixDisplay {
public:
    SoftwareMatrixDisplay(int rows, int cols, int chain_length = 1, int parallel = 1,
                          const std::string& window_name = "rpi-matrix (software)");

    int getWidth() const;
    int getHeight() const;

    // Show what would be displayed on the matrix.
    // Returns last key pressed from the window (OpenCV waitKey), or -1 if none.
    int displayFrame(const cv::Mat& bgr, int delay_ms = 1);

private:
    int rows_;
    int cols_;
    int chain_length_;
    int parallel_;
    std::string window_name_;

    int matrix_w_;
    int matrix_h_;

    cv::Mat matrix_bgr_;
    cv::Mat preview_bgr_;
};

#endif // SOFTWARE_MATRIX_DISPLAY_H
