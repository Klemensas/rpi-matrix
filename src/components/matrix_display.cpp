#include "components/matrix_display.h"
#include <led-matrix.h>
#include <graphics.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <iostream>

using rgb_matrix::RGBMatrix;
using rgb_matrix::RuntimeOptions;
using rgb_matrix::FrameCanvas;
using rgb_matrix::Font;
using rgb_matrix::Color;
using rgb_matrix::DrawText;

MatrixDisplay::MatrixDisplay(int rows, int cols, int chain_length, int parallel,
                             const std::string& hardware_mapping)
    : rows_(rows), cols_(cols), chain_length_(chain_length), 
      parallel_(parallel), hardware_mapping_(hardware_mapping),
      matrix_(nullptr), canvas_(nullptr), font_(nullptr), font_loaded_(false) {
    setup();
    
    // Try to load the smallest legible font for debug overlay (4x6.bdf)
    font_ = new Font();
    // Try to load font from various possible paths
    const char* font_paths[] = {
        "rpi-rgb-led-matrix/fonts/4x6.bdf",
        "../rpi-rgb-led-matrix/fonts/4x6.bdf",
        "../../rpi-rgb-led-matrix/fonts/4x6.bdf",
        "/home/pi/Documents/code/rpi-matrix/rpi-rgb-led-matrix/fonts/4x6.bdf",
        "./rpi-rgb-led-matrix/fonts/4x6.bdf",
        nullptr
    };
    
    for (int i = 0; font_paths[i] != nullptr; i++) {
        if (font_->LoadFont(font_paths[i])) {
            font_loaded_ = true;
            break;
        }
    }
    
    if (!font_loaded_) {
        std::cerr << "Warning: Could not load font for debug overlay. Debug info will not be displayed." << std::endl;
        std::cerr << "Tried paths: rpi-rgb-led-matrix/fonts/6x10.bdf (relative to current directory)" << std::endl;
    }
}

MatrixDisplay::~MatrixDisplay() {
    cleanup();
    if (font_) {
        delete font_;
    }
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
                                  double fps, float temperature_celsius, bool show_debug) {
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
    
    // Draw debug overlay if enabled (before swapping canvas)
    if (show_debug && font_loaded_ && font_) {
        int text_height = font_->height();
        int padding = 1;  // Minimal padding for compact display
        
        // Prepare text strings - use shorter format
        std::ostringstream fps_stream;
        fps_stream << std::fixed << std::setprecision(0) << fps;  // No decimal, no "fps" label
        
        std::ostringstream temp_stream;
        temp_stream << std::fixed << std::setprecision(0) << temperature_celsius << "C";  // No degree symbol, shorter
        
        std::string fps_text = fps_stream.str();
        std::string temp_text = temp_stream.str();
        
        // Draw text with background color for visibility
        Color text_color(255, 255, 0);  // Yellow text
        Color bg_color(0, 0, 0);        // Black background
        
        int y_pos = padding + font_->baseline();
        
        // Draw FPS on first line
        DrawText(canvas_, *font_, padding, y_pos, text_color, &bg_color, fps_text.c_str());
        
        // Draw temperature on second line (if there's space)
        if (matrix_height >= (text_height + padding) * 2) {
            DrawText(canvas_, *font_, padding, y_pos + text_height + padding, text_color, &bg_color, temp_text.c_str());
        } else {
            // If not enough space, draw on same line with minimal spacing
            int fps_width = 0;
            for (size_t i = 0; i < fps_text.length(); i++) {
                fps_width += font_->CharacterWidth(fps_text[i]);
            }
            DrawText(canvas_, *font_, fps_width + padding + 1, y_pos, text_color, &bg_color, temp_text.c_str());
        }
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
