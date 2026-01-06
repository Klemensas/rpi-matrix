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
              << "  --device INDEX         Camera device index (default: 0)\n"
              << "  --video PATH           Use a video file instead of a camera device\n"
              << "  --width WIDTH          Capture width request (default: 640)\n"
              << "  --height HEIGHT        Capture height request (default: 480)\n"
              << "  --rows ROWS            Matrix rows per panel (default: 64)\n"
              << "  --cols COLS            Matrix columns per panel (default: 64)\n"
              << "  --chain CHAIN          Number of chained matrices (default: 1)\n"
              << "  --parallel PARALLEL    Number of parallel chains (default: 1)\n"
              << "  --help                 Show this help message\n"
              << "\n"
              << "Keys:\n"
              << "  1-5  switch display modes\n"
              << "  d    toggle debug info (FPS and temperature)\n"
              << "  q/ESC quit\n"
              << std::endl;
}

// Draw debug overlay on OpenCV Mat (desktop version)
static void drawDebugOverlay(cv::Mat& frame, double fps, float temperature_celsius) {
    if (frame.empty()) return;

    // Prepare debug text
    std::ostringstream fps_text;
    fps_text << "FPS: " << std::fixed << std::setprecision(1) << fps;

    std::ostringstream temp_text;
    temp_text << "Temp: " << std::fixed << std::setprecision(1) << temperature_celsius << "C";

    // Draw text with background for readability
    int font = cv::FONT_HERSHEY_SIMPLEX;
    double font_scale = 0.5;
    int thickness = 1;
    int baseline = 0;

    // FPS text
    cv::Size fps_size = cv::getTextSize(fps_text.str(), font, font_scale, thickness, &baseline);
    cv::Point fps_pos(5, 15);
    cv::rectangle(frame, 
                  cv::Point(fps_pos.x - 2, fps_pos.y - fps_size.height - 2),
                  cv::Point(fps_pos.x + fps_size.width + 2, fps_pos.y + baseline + 2),
                  cv::Scalar(0, 0, 0), -1);  // Black background
    cv::putText(frame, fps_text.str(), fps_pos, font, font_scale, 
                cv::Scalar(0, 255, 0), thickness);  // Green text

    // Temperature text
    cv::Size temp_size = cv::getTextSize(temp_text.str(), font, font_scale, thickness, &baseline);
    cv::Point temp_pos(5, 35);
    cv::rectangle(frame,
                  cv::Point(temp_pos.x - 2, temp_pos.y - temp_size.height - 2),
                  cv::Point(temp_pos.x + temp_size.width + 2, temp_pos.y + baseline + 2),
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
        } else if (strcmp(argv[i], "--rows") == 0 && i + 1 < argc) {
            rows = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--cols") == 0 && i + 1 < argc) {
            cols = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--chain") == 0 && i + 1 < argc) {
            chain_length = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--parallel") == 0 && i + 1 < argc) {
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

    AppCore core(width, height);
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
    std::cout << "  d - Toggle debug info (FPS and temperature)" << std::endl;
    std::cout << "  q/ESC - Quit" << std::endl;

    cv::Mat frame;
    cv::Mat out;

    while (true) {
        if (!cap.read(frame) || frame.empty()) break;

        // Update debug data if enabled
        if (debug_enabled.load()) {
            debug.recordFrame();
        }

        core.processFrame(frame, out);

        // Draw debug overlay if enabled
        if (debug_enabled.load() && !out.empty()) {
            drawDebugOverlay(out, debug.getFPS(), debug.getTemperature());
        }

        int key = display.displayFrame(out, /*delay_ms=*/1);
        if (key == 27 || key == 'q' || key == 'Q') break;

        if (key >= '1' && key <= '5') {
            core.setDisplayMode(key - '0');
            std::cout << "Switched to mode " << (key - '0') << std::endl;
        } else if (key == 'd' || key == 'D') {
            bool new_state = !debug_enabled.load();
            debug_enabled = new_state;
            std::cout << "Debug info " << (new_state ? "enabled" : "disabled") << std::endl;
        }
    }

    return 0;
}
