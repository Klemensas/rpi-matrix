#include "components/camera_capture.h"
#include "components/matrix_display.h"
#include <iostream>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>

volatile bool running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Exiting...\n";
    running = false;
}

// Main orchestrator class
class CameraToMatrix {
public:
    CameraToMatrix(int width, int height, int rows, int cols, 
                   int chain_length = 1, int parallel = 1,
                   const std::string& hardware_mapping = "regular")
        : camera_(width, height),
          matrix_(rows, cols, chain_length, parallel, hardware_mapping) {
    }

    void run() {
        if (!camera_.isReady()) {
            std::cerr << "Camera initialization failed" << std::endl;
            return;
        }

        if (!matrix_.isReady()) {
            std::cerr << "Matrix initialization failed" << std::endl;
            return;
        }

        if (geteuid() == 0) {
            const char* sudo_user = std::getenv("SUDO_USER");
            const char* target_user = sudo_user ? sudo_user : "pi";
            struct passwd *pw = getpwnam(target_user);
            if (pw) {
                setgid(pw->pw_gid);
                setuid(pw->pw_uid);
            }
        }

        // Set up frame processing pipeline: camera -> process -> matrix
        camera_.setFrameCallback([this](uint8_t *data, int width, int height) {
            processFrame(data, width, height);
        });

        // Start camera capture
        camera_.start();

        std::cout << "Camera started. Displaying on LED matrix..." << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;

        // Keep running until interrupted
        while (running) {
            usleep(10000); // 10ms sleep
        }

        camera_.stop();
    }

private:
    // Process frame - this is where you can add OpenCV/MediaPipe processing
    void processFrame(uint8_t *data, int width, int height) {
        // TODO: Add video processing here (OpenCV, MediaPipe, etc.)
        // For now, just pass through to matrix
        
        // Example processing pipeline:
        // 1. Convert to OpenCV Mat if needed
        // 2. Apply filters/effects
        // 3. Run MediaPipe detection
        // 4. Draw overlays
        // 5. Convert back to RGB888
        
        // Display on matrix
        matrix_.displayFrame(data, width, height);
    }

    CameraCapture camera_;
    MatrixDisplay matrix_;
};

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "  --width WIDTH          Camera capture width (default: 640)\n"
              << "  --height HEIGHT        Camera capture height (default: 480)\n"
              << "  --rows ROWS            Matrix rows per panel (default: 64)\n"
              << "  --cols COLS            Matrix columns per panel (default: 64)\n"
              << "  --chain CHAIN          Number of chained matrices (default: 1)\n"
              << "  --parallel PARALLEL    Number of parallel chains (default: 1)\n"
              << "  --hardware-mapping MAP Hardware mapping: regular, adafruit-hat, adafruit-hat-pwm (default: regular)\n"
              << "  --help                 Show this help message\n"
              << std::endl;
}

int main(int argc, char *argv[]) {
    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Default parameters
    int width = 640;
    int height = 480;
    int rows = 64;
    int cols = 64;
    int chain_length = 1;
    int parallel = 1;
    std::string hardware_mapping = "regular";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
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
        } else if (strcmp(argv[i], "--hardware-mapping") == 0 && i + 1 < argc) {
            hardware_mapping = argv[++i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "=" << std::string(60, '=') << std::endl;
    std::cout << "Camera to LED Matrix Display" << std::endl;
    std::cout << "=" << std::string(60, '=') << std::endl;
    std::cout << "Camera resolution: " << width << "x" << height << std::endl;
    std::cout << "Matrix: " << cols << "x" << rows 
              << ", chain=" << chain_length 
              << ", parallel=" << parallel << std::endl;
    std::cout << "Hardware mapping: " << hardware_mapping << std::endl;
    std::cout << "=" << std::string(60, '=') << std::endl;

    CameraToMatrix app(width, height, rows, cols, chain_length, parallel, hardware_mapping);
    app.run();

    std::cout << "Exiting..." << std::endl;
    return 0;
}
