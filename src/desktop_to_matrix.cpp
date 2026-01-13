#include "app/app_core.h"
#include "components/debug_data_collector.h"
#include "components/software_matrix_display.h"

#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cstring>
#include <atomic>
#include <iomanip>
#include <sstream>

static void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "Input options:\n"
              << "  --device INDEX             Camera device index (default: 0)\n"
              << "  --video PATH               Use a video file instead of a camera device\n"
              << "  --width WIDTH              Capture width request (default: 640)\n"
              << "  --height HEIGHT            Capture height request (default: 480)\n"
              << "\n"
              << "Matrix configuration:\n"
              << "  --led-rows ROWS            Matrix rows per panel (default: 64)\n"
              << "  --led-cols COLS            Matrix columns per panel (default: 64)\n"
              << "  --led-chain CHAIN          Number of chained matrices (default: 1)\n"
              << "  --led-parallel PARALLEL    Number of parallel chains (default: 1)\n"
              << "\n"
              << "  --help                     Show this help message\n"
              << "\n"
              << "Keys:\n"
              << "  1-7   switch display modes\n"
              << "  §     toggle multi-panel mode (if --led-chain > 1)\n"
              << "  q     toggle panel layout mode: extend <-> repeat (if --led-chain > 1)\n"
              << "  d     toggle debug info (FPS and temperature)\n"
              << "  ESC   quit\n"
              << std::endl;
}

// Draw debug overlay on OpenCV Mat (desktop version)
// This is designed to work on matrix-resolution images (e.g., 64x64)
static void drawDebugOverlay(cv::Mat& frame, double fps, float temperature_celsius) {
    if (frame.empty()) return;

    // Prepare debug text - shorter format for small matrix
    std::ostringstream fps_text;
    fps_text << std::fixed << std::setprecision(0) << fps;  // Just the number

    std::ostringstream temp_text;
    temp_text << std::fixed << std::setprecision(0) << temperature_celsius << "C";

    // Use smaller font settings appropriate for matrix resolution
    int font = cv::FONT_HERSHEY_SIMPLEX;
    double font_scale = 0.3;  // Smaller for 64x64 display
    int thickness = 1;
    int baseline = 0;

    // FPS text - top left
    cv::Size fps_size = cv::getTextSize(fps_text.str(), font, font_scale, thickness, &baseline);
    cv::Point fps_pos(1, fps_size.height + 1);
    cv::rectangle(frame, 
                  cv::Point(fps_pos.x - 1, fps_pos.y - fps_size.height - 1),
                  cv::Point(fps_pos.x + fps_size.width + 1, fps_pos.y + baseline + 1),
                  cv::Scalar(0, 0, 0), -1);  // Black background
    cv::putText(frame, fps_text.str(), fps_pos, font, font_scale, 
                cv::Scalar(0, 255, 255), thickness);  // Yellow text

    // Temperature text - below FPS
    cv::Size temp_size = cv::getTextSize(temp_text.str(), font, font_scale, thickness, &baseline);
    cv::Point temp_pos(1, fps_pos.y + temp_size.height + 3);
    cv::rectangle(frame,
                  cv::Point(temp_pos.x - 1, temp_pos.y - temp_size.height - 1),
                  cv::Point(temp_pos.x + temp_size.width + 1, temp_pos.y + baseline + 1),
                  cv::Scalar(0, 0, 0), -1);  // Black background
    cv::putText(frame, temp_text.str(), temp_pos, font, font_scale,
                cv::Scalar(0, 255, 255), thickness);  // Yellow text
}

int main(int argc, char *argv[]) {
    int device_index = 0;
    const char* video_path = nullptr;
    int width = 640;
    int height = 480;
    int rows = 64;
    int cols = 64;
    int chain_length = 1;
    int parallel = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            device_index = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--video") == 0 && i + 1 < argc) {
            video_path = argv[++i];
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            width = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            height = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-rows") == 0 && i + 1 < argc) {
            rows = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-cols") == 0 && i + 1 < argc) {
            cols = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-chain") == 0 && i + 1 < argc) {
            chain_length = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-parallel") == 0 && i + 1 < argc) {
            parallel = std::atoi(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    cv::VideoCapture cap;
    if (video_path) {
        cap.open(video_path);
    } else {
        cap.open(device_index);
    }

    if (!cap.isOpened()) {
        std::cerr << "Failed to open video source." << std::endl;
        return 1;
    }

    // Best-effort requests; actual may differ.
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);

    AppCore core(width, height, chain_length);
    DebugDataCollector debug;
    SoftwareMatrixDisplay display(rows, cols, chain_length, parallel);
    std::atomic<bool> debug_enabled(true);

    std::cout << "Desktop runner started. Displaying software matrix preview." << std::endl;
    std::cout << "Display modes:" << std::endl;
    std::cout << "  1 - Default camera (pass-through)" << std::endl;
    std::cout << "  2 - Transformed camera (filled silhouette)" << std::endl;
    std::cout << "  3 - Outline only (wireframe)" << std::endl;
    std::cout << "  4 - Motion Trails (Ghost Effect)" << std::endl;
    std::cout << "  5 - Energy-based Motion (movement adds energy, decays over time)" << std::endl;
    std::cout << "  6 - Rainbow Motion Trails (camera + colorful movement paths)" << std::endl;
    std::cout << "  7 - Double Exposure (time-based with randomization)" << std::endl;
    std::cout << "  8 - Procedural Shapes (colorful morphing geometric shapes)" << std::endl;
    std::cout << "\nControls:" << std::endl;
    if (chain_length > 1) {
        std::cout << "  § - Toggle multi-panel mode (apply different effects per panel)" << std::endl;
        std::cout << "  q - Toggle panel layout (extend: span image | repeat: same image)" << std::endl;
    }
    std::cout << "  d - Toggle debug info (FPS and temperature)" << std::endl;
    std::cout << "  ESC - Quit" << std::endl;

    cv::Mat frame;
    cv::Mat out;

    while (true) {
        if (!cap.read(frame) || frame.empty()) break;

        // Update debug data if enabled
        if (debug_enabled.load()) {
            debug.recordFrame();
        }

        core.processFrame(frame, out);

        // Create overlay callback if debug is enabled
        std::function<void(cv::Mat&)> overlay_callback = nullptr;
        if (debug_enabled.load()) {
            overlay_callback = [&debug](cv::Mat& matrix_frame) {
                drawDebugOverlay(matrix_frame, debug.getFPS(), debug.getTemperature());
            };
        }

        int key = display.displayFrame(out, /*delay_ms=*/1, overlay_callback);
        if (key == 27) break;  // ESC to quit

        if (key >= '1' && key <= '8') {
            core.setDisplayMode(key - '0');
            std::cout << "Switched to mode " << (key - '0') << std::endl;
        } else if (key == 'd' || key == 'D') {
            bool new_state = !debug_enabled.load();
            debug_enabled = new_state;
            std::cout << "Debug info " << (new_state ? "enabled" : "disabled") << std::endl;
        } else if (key == 'q' || key == 'Q') {
            // Toggle panel mode (extend <-> repeat)
            PanelMode current = core.getPanelMode();
            PanelMode new_mode = (current == PanelMode::EXTEND) ? PanelMode::REPEAT : PanelMode::EXTEND;
            core.setPanelMode(new_mode);
            const char* mode_name = (new_mode == PanelMode::EXTEND) ? "EXTEND" : "REPEAT";
            std::cout << "Panel layout mode: " << mode_name << std::endl;
            if (new_mode == PanelMode::EXTEND) {
                std::cout << "  (Image spans across all panels)" << std::endl;
            } else {
                std::cout << "  (Same image on each panel with different effects)" << std::endl;
            }
        } else if (key == 'a' || key == 'A') {
            // Toggle auto-cycling
            core.toggleAutoCycling();
            bool enabled = core.isAutoCycling();
            std::cout << "Auto-cycling " << (enabled ? "enabled" : "disabled") << std::endl;
            if (enabled) {
                std::cout << "  (Modes will automatically cycle every 3-7 seconds)" << std::endl;
            }
        }
    }

    return 0;
}
