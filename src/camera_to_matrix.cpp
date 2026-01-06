#include "components/camera_capture.h"
#include "components/matrix_display.h"
#include "components/debug_overlay.h"
#include "components/debug_data_collector.h"
#include "app/app_core.h"
#include <led-matrix.h>
#include <opencv2/core.hpp>
#include <iostream>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <atomic>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <vector>
#include <functional>
#include <cmath>

using rgb_matrix::FrameCanvas;

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
          matrix_(rows, cols, chain_length, parallel, hardware_mapping),
          debug_overlay_(),
          debug_data_collector_(),
          debug_enabled_(true),
          core_(width, height) {}

    void run() {
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

        // Set up keyboard input (non-blocking)
        setupKeyboardInput();

        std::cout << "Camera started. Displaying on LED matrix..." << std::endl;
        std::cout << "Display modes:" << std::endl;
        std::cout << "  1 - Default camera (pass-through)" << std::endl;
        std::cout << "  2 - Transformed camera (filled silhouette)" << std::endl;
        std::cout << "  3 - Outline only (wireframe)" << std::endl;
        std::cout << "  4 - Motion Trails (Ghost Effect)" << std::endl;
        std::cout << "  5 - Energy-based Motion (movement adds energy, decays over time)" << std::endl;
        std::cout << "  d - Toggle debug info (FPS and temperature)" << std::endl;
        std::cout << "Press 1-5 or d to switch modes, Ctrl+C to stop" << std::endl;

        // Keep running until interrupted
        while (running) {
            checkKeyboardInput();
            usleep(10000); // 10ms sleep
        }

        restoreKeyboardInput();

        camera_.stop();
    }

private:
    // Process frame - routes to appropriate display mode
    void processFrame(uint8_t *data, int width, int height) {
        bool debug = debug_enabled_.load();
        
        // Only update debug data collection when debug mode is enabled
        if (debug) {
            debug_data_collector_.recordFrame();
        }
        
        // Create overlay callback if debug is enabled
        std::function<void(FrameCanvas*)> overlay_callback = nullptr;
        if (debug && debug_overlay_.isReady()) {
            overlay_callback = [this](FrameCanvas* canvas) {
                debug_overlay_.draw(canvas, 
                                   debug_data_collector_.getFPS(),
                                   debug_data_collector_.getTemperature());
            };
        }

        // libcamera stream is configured as RGB888, but in practice is BGR byte-order in this pipeline.
        // Treat input as BGR consistently with OpenCV.
        cv::Mat in_bgr(height, width, CV_8UC3, data);
        cv::Mat out_bgr;
        core_.processFrame(in_bgr, out_bgr);
        if (!out_bgr.empty()) {
            matrix_.displayFrame(out_bgr.data, out_bgr.cols, out_bgr.rows, overlay_callback);
        }
    }

    void setupKeyboardInput() {
        // Save current terminal settings
        tcgetattr(STDIN_FILENO, &original_termios_);
        
        // Set terminal to raw mode (non-canonical, no echo)
        struct termios raw = original_termios_;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;  // Non-blocking read
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        
        // Set stdin to non-blocking
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    void restoreKeyboardInput() {
        // Restore original terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios_);
    }

    void checkKeyboardInput() {
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        
        if (select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout) > 0) {
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                char key;
                if (read(STDIN_FILENO, &key, 1) == 1) {
                    if (key == '1') {
                        core_.setDisplayMode(1);
                        std::cout << "Switched to mode 1: Default camera" << std::endl;
                    } else if (key == '2') {
                        core_.setDisplayMode(2);
                        std::cout << "Switched to mode 2: Transformed camera (filled silhouette)" << std::endl;
                    } else if (key == '3') {
                        core_.setDisplayMode(3);
                        std::cout << "Switched to mode 3: Outline only (wireframe)" << std::endl;
                    } else if (key == '4') {
                        core_.setDisplayMode(4);
                        std::cout << "Switched to mode 4: Motion Trails (Ghost Effect)" << std::endl;
                    } else if (key == '5') {
                        core_.setDisplayMode(5);
                        std::cout << "Switched to mode 5: Energy-based Motion" << std::endl;
                    } else if (key == 'd' || key == 'D') {
                        bool new_state = !debug_enabled_.load();
                        debug_enabled_ = new_state;
                        std::cout << "Debug info " << (new_state ? "enabled" : "disabled") << std::endl;
                    }
                }
            }
        }
    }

    CameraCapture camera_;
    MatrixDisplay matrix_;
    DebugOverlay debug_overlay_;
    DebugDataCollector debug_data_collector_;
    std::atomic<bool> debug_enabled_;  // Thread-safe debug toggle
    struct termios original_termios_;  // For restoring terminal settings

    AppCore core_;
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
