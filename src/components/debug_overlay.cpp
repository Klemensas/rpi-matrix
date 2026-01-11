#include "components/debug_overlay.h"
#include <graphics.h>
#include <sstream>
#include <iomanip>
#include <iostream>

using rgb_matrix::FrameCanvas;
using rgb_matrix::Font;
using rgb_matrix::Color;
using rgb_matrix::DrawText;

DebugOverlay::DebugOverlay() : font_(nullptr), font_loaded_(false) {
    loadFont();
}

DebugOverlay::~DebugOverlay() {
    if (font_) {
        delete font_;
    }
}

bool DebugOverlay::isReady() const {
    return font_loaded_ && font_ != nullptr;
}

void DebugOverlay::loadFont() {
    font_ = new Font();
    // Try to load the smallest legible font from various possible paths
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
        std::cerr << "Tried paths: rpi-rgb-led-matrix/fonts/4x6.bdf (relative to current directory)" << std::endl;
    }
}

void DebugOverlay::draw(FrameCanvas *canvas, double fps, float temperature_celsius) {
    if (!canvas || !font_loaded_ || !font_) return;
    
    int matrix_width = canvas->width();
    int matrix_height = canvas->height();
    int text_height = font_->height();
    int padding = 1;  // Minimal padding for compact display
    
    // Prepare text strings - use shorter format
    std::ostringstream fps_stream;
    fps_stream << std::fixed << std::setprecision(0) << fps << "fps";
    
    std::ostringstream temp_stream;
    temp_stream << std::fixed << std::setprecision(0) << temperature_celsius << "C";  // No degree symbol, shorter
    
    std::string fps_text = fps_stream.str();
    std::string temp_text = temp_stream.str();
    
    // Draw text with background color for visibility
    Color text_color(255, 255, 0);  // Yellow text
    Color bg_color(0, 0, 0);        // Black background
    
    int y_pos = padding + font_->baseline();
    
    // Draw FPS on first line
    DrawText(canvas, *font_, padding, y_pos, text_color, &bg_color, fps_text.c_str());
    
    // Draw temperature on second line (if there's space)
    if (matrix_height >= (text_height + padding) * 2) {
        DrawText(canvas, *font_, padding, y_pos + text_height + padding, text_color, &bg_color, temp_text.c_str());
    } else {
        // If not enough space, draw on same line with minimal spacing
        int fps_width = 0;
        for (size_t i = 0; i < fps_text.length(); i++) {
            fps_width += font_->CharacterWidth(fps_text[i]);
        }
        DrawText(canvas, *font_, fps_width + padding + 1, y_pos, text_color, &bg_color, temp_text.c_str());
    }
}
