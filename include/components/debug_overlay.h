#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include <led-matrix.h>
#include <graphics.h>

using rgb_matrix::FrameCanvas;
using rgb_matrix::Font;

class DebugOverlay {
public:
    DebugOverlay();
    ~DebugOverlay();

    // Check if overlay is ready (font loaded)
    bool isReady() const;

    // Draw debug overlay on the canvas
    // This should be called before SwapOnVSync
    void draw(FrameCanvas *canvas, double fps, float temperature_celsius);

private:
    Font *font_;
    bool font_loaded_;
    
    void loadFont();
};

#endif // DEBUG_OVERLAY_H
