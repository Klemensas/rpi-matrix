#include "app/app_core.h"
#include "components/debug_data_collector.h"
#include "components/software_matrix_display.h"

#include <opencv2/videoio.hpp>
#include <iostream>
#include <cstring>

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
              << "  q/ESC quit\n"
              << std::endl;
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

    std::cout << "Desktop runner started. Displaying software matrix preview." << std::endl;
    std::cout << "Modes: 1=pass-through, 2=filled silhouette, 3=outline, 4=trails, 5=energy" << std::endl;

    cv::Mat frame;
    cv::Mat out;

    while (true) {
        if (!cap.read(frame) || frame.empty()) break;

        debug.recordFrame();
        core.processFrame(frame, out);

        int key = display.displayFrame(out, /*delay_ms=*/1);
        if (key == 27 || key == 'q' || key == 'Q') break;

        if (key >= '1' && key <= '5') {
            core.setDisplayMode(key - '0');
            std::cout << "Switched to mode " << (key - '0') << std::endl;
        }
    }

    return 0;
}
