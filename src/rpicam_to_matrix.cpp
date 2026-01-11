#include <iostream>
#include <memory>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <led-matrix.h>

using rgb_matrix::RGBMatrix;
using rgb_matrix::RuntimeOptions;
using rgb_matrix::FrameCanvas;

volatile bool running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Exiting...\n";
    running = false;
}

class RpicamToMatrix {
public:
    RpicamToMatrix(int rows, int cols, 
                   int chain_length = 1, int parallel = 1,
                   const std::string& hardware_mapping = "regular",
                   int brightness = 50, int gpio_slowdown = 4,
                   int pwm_bits = 11, int pwm_lsb_nanoseconds = 130,
                   int limit_refresh_rate_hz = 0)
        : rows_(rows), cols_(cols),
          chain_length_(chain_length), parallel_(parallel),
          hardware_mapping_(hardware_mapping),
          brightness_(brightness), gpio_slowdown_(gpio_slowdown),
          pwm_bits_(pwm_bits), pwm_lsb_nanoseconds_(pwm_lsb_nanoseconds),
          limit_refresh_rate_hz_(limit_refresh_rate_hz) {
        setupMatrix();
    }

    ~RpicamToMatrix() {
        cleanup();
    }

    bool initialize() {
        if (!matrix_) {
            std::cerr << "Failed to create RGB matrix" << std::endl;
            return false;
        }
        return true;
    }

    void run(int input_width, int input_height) {
        if (!initialize()) {
            std::cerr << "Failed to initialize matrix" << std::endl;
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

        int matrix_width = canvas_->width();
        int matrix_height = canvas_->height();
        
        if (!canvas_) {
            std::cerr << "ERROR: Canvas is null!" << std::endl;
            return;
        }

        std::cerr << "Matrix initialized successfully!" << std::endl;
        std::cerr << "Matrix size: " << matrix_width << "x" << matrix_height << std::endl;
        std::cerr << "Reading from stdin..." << std::endl;
        std::cerr << "Input resolution: " << input_width << "x" << input_height << std::endl;
        std::cerr << "Frame size: " << (input_width * input_height * 3) << " bytes" << std::endl;
        std::cerr << "Press Ctrl+C to stop" << std::endl;

        // Disable buffering on stdin for real-time reading
        setvbuf(stdin, NULL, _IONBF, 0);
        std::cin.sync_with_stdio(false);
        std::cin.tie(nullptr);

        // Read raw RGB888 frames from stdin
        size_t frame_size = input_width * input_height * 3;
        std::vector<uint8_t> frame_buffer(frame_size);
        size_t frame_count = 0;

        while (running) {
            // Read one complete frame (raw RGB888)
            std::cin.read(reinterpret_cast<char*>(frame_buffer.data()), frame_size);
            size_t bytes_read = std::cin.gcount();
            
            if (bytes_read == 0) {
                if (std::cin.eof()) {
                    std::cerr << "End of input (EOF)" << std::endl;
                    break;
                }
                if (std::cin.fail()) {
                    std::cerr << "Read error or no data available" << std::endl;
                    usleep(10000); // Wait a bit before retrying
                    continue;
                }
                break;
            }

            if (bytes_read < frame_size) {
                std::cerr << "Warning: Incomplete frame (" << bytes_read 
                          << " bytes, expected " << frame_size << ")" << std::endl;
                // Try to read the rest
                std::cin.read(reinterpret_cast<char*>(frame_buffer.data() + bytes_read), 
                             frame_size - bytes_read);
                bytes_read += std::cin.gcount();
                if (bytes_read < frame_size) {
                    std::cerr << "Still incomplete, skipping frame" << std::endl;
                    continue;
                }
            }

            // Display frame on matrix
            displayFrame(frame_buffer.data(), input_width, input_height);
            frame_count++;
            
            if (frame_count % 30 == 0) {
                std::cerr << "Processed " << frame_count << " frames..." << std::endl;
            }
        }

        std::cerr << "Total frames processed: " << frame_count << std::endl;

        std::cout << "End of input" << std::endl;
    }

private:
    void setupMatrix() {
        RGBMatrix::Options options;
        options.rows = rows_;
        options.cols = cols_;
        options.chain_length = chain_length_;
        options.parallel = parallel_;
        options.hardware_mapping = hardware_mapping_.c_str();
        options.brightness = brightness_;
        options.pwm_bits = pwm_bits_;
        options.pwm_lsb_nanoseconds = pwm_lsb_nanoseconds_;
        options.limit_refresh_rate_hz = limit_refresh_rate_hz_;
        options.disable_hardware_pulsing = true;
        options.show_refresh_rate = true;
        
        RuntimeOptions runtime_options;
        runtime_options.drop_privileges = 0;
        runtime_options.gpio_slowdown = gpio_slowdown_;
        
        matrix_ = RGBMatrix::CreateFromOptions(options, runtime_options);
        if (matrix_) {
            canvas_ = matrix_->CreateFrameCanvas();
        }
    }

    void displayFrame(uint8_t *data, int width, int height) {
        if (!canvas_) {
            std::cerr << "ERROR: Canvas is null in displayFrame!" << std::endl;
            return;
        }

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

                // FFmpeg outputs RGB888, use directly
                canvas_->SetPixel(x, y, r, g, b);
            }
        }

        canvas_ = matrix_->SwapOnVSync(canvas_);
    }

    void cleanup() {
        if (matrix_) {
            matrix_->Clear();
        }
    }

    int rows_;
    int cols_;
    int chain_length_;
    int parallel_;
    std::string hardware_mapping_;
    int brightness_;
    int gpio_slowdown_;
    int pwm_bits_;
    int pwm_lsb_nanoseconds_;
    int limit_refresh_rate_hz_;
    RGBMatrix *matrix_;
    FrameCanvas *canvas_;
};

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "Input options:\n"
              << "  --width WIDTH                  Input video width (default: 640)\n"
              << "  --height HEIGHT                Input video height (default: 480)\n"
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
              << "  --led-pwm-lsb-nanoseconds N    PWM LSB nanoseconds (default: 130, range: 50-3000)\n"
              << "                                 Lower values = higher refresh rate, more ghosting\n"
              << "  --led-limit-refresh N          Limit refresh rate to N Hz (default: 0 = no limit)\n"
              << "\n"
              << "  --help                         Show this help message\n"
              << "\n"
              << "Reads raw RGB888 frames from stdin and displays on LED matrix.\n"
              << "\n"
              << "Example with rpicam-vid:\n"
              << "  rpicam-vid -t 0 --width 640 --height 480 --codec yuv420 -o - | \\\n"
              << "    ffmpeg -loglevel error -f rawvideo -pix_fmt yuv420p -s 640x480 -r 30 -i - \\\n"
              << "    -f rawvideo -pix_fmt rgb24 - | \\\n"
              << "    sudo " << program << " --width 640 --height 480\n"
              << std::endl;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    int input_width = 640;
    int input_height = 480;
    int rows = 64;
    int cols = 64;
    int chain_length = 1;
    int parallel = 1;
    std::string hardware_mapping = "regular";
    int brightness = 50;
    int gpio_slowdown = 4;
    int pwm_bits = 11;
    int pwm_lsb_nanoseconds = 130;
    int limit_refresh_rate_hz = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            input_width = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            input_height = std::atoi(argv[++i]);
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
    std::cout << "Rpicam to LED Matrix Display" << std::endl;
    std::cout << "=" << std::string(60, '=') << std::endl;
    std::cout << "Input resolution: " << input_width << "x" << input_height << std::endl;
    std::cout << "Matrix: " << cols << "x" << rows 
              << ", chain=" << chain_length 
              << ", parallel=" << parallel << std::endl;
    std::cout << "Hardware mapping: " << hardware_mapping << std::endl;
    std::cout << "Display settings: brightness=" << brightness
              << ", pwm-bits=" << pwm_bits
              << ", pwm-lsb-ns=" << pwm_lsb_nanoseconds << std::endl;
    std::cout << "Performance: gpio-slowdown=" << gpio_slowdown;
    if (limit_refresh_rate_hz > 0) {
        std::cout << ", refresh-limit=" << limit_refresh_rate_hz << "Hz";
    }
    std::cout << std::endl;
    std::cout << "=" << std::string(60, '=') << std::endl;

    RpicamToMatrix app(rows, cols, chain_length, parallel, hardware_mapping,
                       brightness, gpio_slowdown, pwm_bits, pwm_lsb_nanoseconds,
                       limit_refresh_rate_hz);
    app.run(input_width, input_height);

    std::cout << "Exiting..." << std::endl;
    return 0;
}
