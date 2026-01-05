#include "components/matrix_display.h"
#include <led-matrix.h>

using rgb_matrix::RGBMatrix;
using rgb_matrix::RuntimeOptions;
using rgb_matrix::FrameCanvas;

MatrixDisplay::MatrixDisplay(int rows, int cols, int chain_length, int parallel,
                             const std::string& hardware_mapping)
    : rows_(rows), cols_(cols), chain_length_(chain_length), 
      parallel_(parallel), hardware_mapping_(hardware_mapping),
      matrix_(nullptr), canvas_(nullptr) {
    setup();
}

MatrixDisplay::~MatrixDisplay() {
    cleanup();
}

bool MatrixDisplay::isReady() const {
    return matrix_ != nullptr && canvas_ != nullptr;
}

int MatrixDisplay::getWidth() const {
    return canvas_ ? canvas_->width() : 0;
}

int MatrixDisplay::getHeight() const {
    return canvas_ ? canvas_->height() : 0;
}

void MatrixDisplay::displayFrame(uint8_t *data, int width, int height, 
                                  std::function<void(FrameCanvas*)> overlay_callback) {
    if (!canvas_) return;

    int matrix_width = canvas_->width();
    int matrix_height = canvas_->height();

    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            int src_x = (x * width) / matrix_width;
            int src_y = (y * height) / matrix_height;
            
            int src_idx = (src_y * width + src_x) * 3;
            uint8_t r = data[src_idx];
            uint8_t g = data[src_idx + 1];
            uint8_t b = data[src_idx + 2];

            // Camera outputs BGR, swap to RGB
            canvas_->SetPixel(x, y, b, g, r);
        }
    }
    
    // Call overlay callback if provided (before swapping canvas)
    if (overlay_callback) {
        overlay_callback(canvas_);
    }

    canvas_ = matrix_->SwapOnVSync(canvas_);
}

void MatrixDisplay::setup() {
    RGBMatrix::Options options;
    options.rows = rows_;
    options.cols = cols_;
    options.chain_length = chain_length_;
    options.parallel = parallel_;
    options.hardware_mapping = hardware_mapping_.c_str();
    options.brightness = 50;
    options.disable_hardware_pulsing = true;
    
    RuntimeOptions runtime_options;
    runtime_options.drop_privileges = 0;
    runtime_options.gpio_slowdown = 4;
    
    matrix_ = RGBMatrix::CreateFromOptions(options, runtime_options);
    if (matrix_) {
        canvas_ = matrix_->CreateFrameCanvas();
    }
}

void MatrixDisplay::cleanup() {
    if (matrix_) {
        matrix_->Clear();
    }
}
