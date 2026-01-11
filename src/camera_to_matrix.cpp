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
                   const std::string& hardware_mapping = "regular",
                   int brightness = 50, int gpio_slowdown = 4,
                   int pwm_bits = 11, int pwm_dither_bits = 0,
                   int pwm_lsb_nanoseconds = 130,
                   int limit_refresh_rate_hz = 0,
                   int sensor_width = 0, int sensor_height = 0)
        : camera_(width, height, sensor_width, sensor_height),
          matrix_(rows, cols, chain_length, parallel, hardware_mapping,
                  brightness, gpio_slowdown, pwm_bits, pwm_dither_bits,
                  pwm_lsb_nanoseconds, limit_refresh_rate_hz),
          debug_overlay_(),
          debug_data_collector_(),
          debug_enabled_(true),
          core_(width, height, chain_length) {}

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
        std::cout << "\nMulti-Panel Mode (independent of display modes):" << std::endl;
        int num_panels = core_.getNumPanels();
        if (num_panels > 1) {
            std::cout << "  § - Toggle multi-panel mode and cycle target (P1";
            for (int i = 2; i <= num_panels; i++) {
                std::cout << " -> P" << i;
            }
            std::cout << " -> All -> Off)" << std::endl;
            std::cout << "      When enabled, 1-5 keys apply effects to targeted panel(s)" << std::endl;
        } else {
            std::cout << "  (Multi-panel mode requires --led-chain > 1)" << std::endl;
        }
        std::cout << "\nOther controls:" << std::endl;
        std::cout << "  d - Toggle debug info (FPS and CPU temperature)" << std::endl;
        std::cout << "Press 1-5, §, or d; Ctrl+C to stop" << std::endl;

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
        // Note: If sensor mode was specified, libcamera's ISP handles scaling (hardware-accelerated)
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
                    bool in_multi_panel = multi_panel_enabled_.load();
                    
                    // § key (section sign, 0xC2 0xA7 in UTF-8)
                    if (key == static_cast<char>(0xC2)) {
                        // Read next byte for UTF-8 multi-byte character
                        char key2;
                        if (read(STDIN_FILENO, &key2, 1) == 1 && key2 == static_cast<char>(0xA7)) {
                            int num_panels = core_.getNumPanels();
                            
                            if (!in_multi_panel) {
                                // Enter multi-panel mode, target Panel 1
                                // Initialize all panels to the current display mode
                                int current_mode = core_.displayMode();
                                for (int i = 0; i < num_panels; i++) {
                                    core_.setPanelEffect(i, current_mode);
                                }
                                multi_panel_enabled_ = true;
                                core_.setMultiPanelEnabled(true);
                                panel_target_ = 0;
                                std::cout << "Multi-Panel Mode ENABLED - Target: Panel 1" << std::endl;
                                std::cout << "(All panels start with current mode " << current_mode << ")" << std::endl;
                            } else {
                                // Cycle through targets: 0 (P1) -> ... -> (Pn) -> -1 (all) -> off
                                int current_target = panel_target_.load();
                                
                                if (current_target == -1) {
                                    // After "all", disable multi-panel mode
                                    multi_panel_enabled_ = false;
                                    core_.setMultiPanelEnabled(false);
                                    std::cout << "Multi-Panel Mode DISABLED" << std::endl;
                                } else if (current_target == num_panels - 1) {
                                    // After last panel, go to "all"
                                    panel_target_ = -1;
                                    std::cout << "Target: All panels" << std::endl;
                                } else {
                                    // Go to next panel
                                    panel_target_ = current_target + 1;
                                    std::cout << "Target: Panel " << (current_target + 2) << std::endl;
                                }
                            }
                        }
                    } else if (key >= '1' && key <= '5') {
                        int effect = key - '0';
                        
                        if (in_multi_panel) {
                            // Apply effect to current target panel(s)
                            int target = panel_target_.load();
                            int num_panels = core_.getNumPanels();
                            if (target == -1) {
                                // Apply to all panels
                                for (int i = 0; i < num_panels; i++) {
                                    core_.setPanelEffect(i, effect);
                                }
                                std::cout << "Applied effect " << effect << " to all panels" << std::endl;
                            } else {
                                // Apply to specific panel
                                core_.setPanelEffect(target, effect);
                                std::cout << "Applied effect " << effect << " to Panel " << (target + 1) << std::endl;
                            }
                        } else {
                            // Regular mode switching (affects all panels uniformly)
                            core_.setDisplayMode(effect);
                            const char* mode_names[] = {
                                "",
                                "Default camera",
                                "Transformed camera (filled silhouette)",
                                "Outline only (wireframe)",
                                "Motion Trails (Ghost Effect)",
                                "Energy-based Motion"
                            };
                            std::cout << "Switched to mode " << effect << ": " << mode_names[effect] << std::endl;
                        }
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
    
    // Multi-panel state: independent of display modes
    std::atomic<bool> multi_panel_enabled_{false};
    std::atomic<int> panel_target_{0};  // 0/1/2 = specific panel, -1 = all panels
};

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "Camera options:\n"
              << "  --width WIDTH                  Output width for processing (default: 640)\n"
              << "  --height HEIGHT                Output height for processing (default: 480)\n"
              << "  --sensor-width WIDTH           Sensor capture width for FOV control (default: auto)\n"
              << "                                 Larger = wider field of view (less zoom)\n"
              << "                                 e.g., --sensor-width 2304 --sensor-height 1296\n"
              << "  --sensor-height HEIGHT         Sensor capture height for FOV control (default: auto)\n"
              << "\n"
              << "Matrix configuration:\n"
              << "  --led-rows ROWS                Matrix rows per panel (default: 64)\n"
              << "  --led-cols COLS                Matrix columns per panel (default: 64)\n"
              << "  --led-chain CHAIN              Number of chained matrices (default: 1)\n"
              << "  --led-parallel PARALLEL        Number of parallel chains (default: 1)\n"
              << "  --led-hardware-mapping MAP     Hardware mapping: regular, adafruit-hat, adafruit-hat-pwm (default: regular)\n"
              << "\n"
              << "Matrix performance tuning:\n"
              << "  --led-brightness N             LED brightness 0-100 (default: 50)\n"
              << "  --led-slowdown-gpio N          GPIO slowdown for stability (default: 4, try 2-4)\n"
              << "  --led-pwm-bits N               PWM bits for color depth (default: 11, range: 1-11)\n"
              << "                                 Lower values = less CPU, higher refresh rate, fewer colors\n"
              << "  --led-pwm-dither-bits N        Dither bits for temporal dithering (default: 0, range: 0-2)\n"
              << "                                 Time-dithering of lower bits for smoother color\n"
              << "  --led-pwm-lsb-nanoseconds N    PWM LSB nanoseconds (default: 130, range: 50-3000)\n"
              << "                                 Lower values = higher refresh rate, more ghosting\n"
              << "  --led-limit-refresh N          Limit refresh rate to N Hz (default: 0 = no limit)\n"
              << "\n"
              << "  --help                         Show this help message\n"
              << std::endl;
}

int main(int argc, char *argv[]) {
    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Default parameters
    int width = 640;
    int height = 480;
    int sensor_width = 0;  // 0 = auto
    int sensor_height = 0;
    int rows = 64;
    int cols = 64;
    int chain_length = 1;
    int parallel = 1;
    std::string hardware_mapping = "regular";
    int brightness = 50;
    int gpio_slowdown = 4;
    int pwm_bits = 11;
    int pwm_dither_bits = 0;
    int pwm_lsb_nanoseconds = 130;
    int limit_refresh_rate_hz = 0;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            width = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            height = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--sensor-width") == 0 && i + 1 < argc) {
            sensor_width = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--sensor-height") == 0 && i + 1 < argc) {
            sensor_height = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-rows") == 0 && i + 1 < argc) {
            rows = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-cols") == 0 && i + 1 < argc) {
            cols = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-chain") == 0 && i + 1 < argc) {
            chain_length = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-parallel") == 0 && i + 1 < argc) {
            parallel = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-hardware-mapping") == 0 && i + 1 < argc) {
            hardware_mapping = argv[++i];
        } else if (strcmp(argv[i], "--led-brightness") == 0 && i + 1 < argc) {
            brightness = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-slowdown-gpio") == 0 && i + 1 < argc) {
            gpio_slowdown = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-pwm-bits") == 0 && i + 1 < argc) {
            pwm_bits = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-pwm-dither-bits") == 0 && i + 1 < argc) {
            pwm_dither_bits = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-pwm-lsb-nanoseconds") == 0 && i + 1 < argc) {
            pwm_lsb_nanoseconds = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--led-limit-refresh") == 0 && i + 1 < argc) {
            limit_refresh_rate_hz = std::atoi(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "=" << std::string(60, '=') << std::endl;
    std::cout << "Camera to LED Matrix Display" << std::endl;
    std::cout << "=" << std::string(60, '=') << std::endl;
    std::cout << "Output resolution: " << width << "x" << height << std::endl;
    if (sensor_width > 0 && sensor_height > 0) {
        std::cout << "Sensor capture: " << sensor_width << "x" << sensor_height 
                  << " (for FOV control, will scale to output)" << std::endl;
    }
    std::cout << "Matrix: " << cols << "x" << rows 
              << ", chain=" << chain_length 
              << ", parallel=" << parallel << std::endl;
    std::cout << "Hardware mapping: " << hardware_mapping << std::endl;
    std::cout << "Display settings: brightness=" << brightness
              << ", pwm-bits=" << pwm_bits
              << ", pwm-dither=" << pwm_dither_bits
              << ", pwm-lsb-ns=" << pwm_lsb_nanoseconds << std::endl;
    std::cout << "Performance: gpio-slowdown=" << gpio_slowdown;
    if (limit_refresh_rate_hz > 0) {
        std::cout << ", refresh-limit=" << limit_refresh_rate_hz << "Hz";
    }
    std::cout << std::endl;
    std::cout << "=" << std::string(60, '=') << std::endl;

    CameraToMatrix app(width, height, rows, cols, chain_length, parallel, 
                       hardware_mapping, brightness, gpio_slowdown,
                       pwm_bits, pwm_dither_bits, pwm_lsb_nanoseconds, 
                       limit_refresh_rate_hz, sensor_width, sensor_height);
    app.run();

    std::cout << "Exiting..." << std::endl;
    return 0;
}
