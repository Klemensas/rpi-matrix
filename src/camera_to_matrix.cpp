#include <iostream>
#include <memory>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>
#include <pwd.h>
#include <libcamera/libcamera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/camera.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <led-matrix.h>

using namespace libcamera;
using rgb_matrix::RGBMatrix;
using rgb_matrix::RuntimeOptions;
using rgb_matrix::FrameCanvas;

volatile bool running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Exiting...\n";
    running = false;
}

class CameraToMatrix {
public:
    CameraToMatrix(int width, int height, int rows, int cols, 
                   int chain_length = 1, int parallel = 1,
                   const std::string& hardware_mapping = "regular")
        : width_(width), height_(height), rows_(rows), cols_(cols),
          chain_length_(chain_length), parallel_(parallel),
          hardware_mapping_(hardware_mapping) {
        setupMatrix();
        // Setup camera manager while still root (needed for DMA buffers)
        // But don't acquire camera yet - that happens in run()
        setupCameraManager();
    }

    ~CameraToMatrix() {
        cleanup();
    }

    bool initialize() {
        if (!matrix_) {
            std::cerr << "Failed to create RGB matrix" << std::endl;
            return false;
        }

        if (!camera_) {
            std::cerr << "Failed to open camera" << std::endl;
            return false;
        }

        return true;
    }

    void run() {
        // Acquire camera while still root (needed for DMA buffer access)
        acquireCamera();
        
        // Now drop privileges after camera is acquired
        uid_t current_uid = geteuid();
        if (current_uid == 0) {
            std::cout << "Dropping privileges after camera acquisition..." << std::endl;
            const char* sudo_user = std::getenv("SUDO_USER");
            const char* target_user = sudo_user ? sudo_user : "pi";
            struct passwd *pw = getpwnam(target_user);
            if (pw) {
                if (setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
                    std::cerr << "Failed to drop privileges to " << target_user << std::endl;
                    return;
                }
                std::cout << "Successfully dropped to " << target_user 
                          << " (UID: " << geteuid() << ", GID: " << getegid() << ")" << std::endl;
            } else {
                std::cerr << "User " << target_user << " not found" << std::endl;
                return;
            }
        }
        
        if (!initialize()) {
            return;
        }

        // Configure camera stream
        std::unique_ptr<CameraConfiguration> config = camera_->generateConfiguration({StreamRole::Viewfinder});
        if (!config) {
            std::cerr << "Failed to generate camera configuration" << std::endl;
            return;
        }

        // Set stream format
        StreamConfiguration &streamConfig = config->at(0);
        streamConfig.size.width = width_;
        streamConfig.size.height = height_;
        streamConfig.pixelFormat = formats::RGB888;
        streamConfig.bufferCount = 2;

        config->validate();
        if (camera_->configure(config.get()) < 0) {
            std::cerr << "Failed to configure camera" << std::endl;
            return;
        }

        // Allocate buffers
        FrameBufferAllocator *allocator = new FrameBufferAllocator(camera_);
        for (StreamConfiguration &cfg : *config) {
            int ret = allocator->allocate(cfg.stream());
            if (ret < 0) {
                std::cerr << "Failed to allocate buffers" << std::endl;
                delete allocator;
                return;
            }
        }

        // Start camera
        if (camera_->start()) {
            std::cerr << "Failed to start camera" << std::endl;
            delete allocator;
            return;
        }

        // Create requests
        std::vector<std::unique_ptr<Request>> requests;
        for (const StreamConfiguration &cfg : *config) {
            const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator->buffers(cfg.stream());
            for (unsigned int i = 0; i < buffers.size(); ++i) {
                std::unique_ptr<Request> request = camera_->createRequest();
                if (!request) {
                    std::cerr << "Can't create request" << std::endl;
                    delete allocator;
                    return;
                }

                const std::unique_ptr<FrameBuffer> &buffer = buffers[i];
                int ret = request->addBuffer(cfg.stream(), buffer.get());
                if (ret < 0) {
                    std::cerr << "Can't set buffer for request" << std::endl;
                    delete allocator;
                    return;
                }

                requests.push_back(std::move(request));
            }
        }

        // Queue all requests
        for (auto &request : requests) {
            camera_->queueRequest(request.get());
        }

        std::cout << "Camera started. Displaying on LED matrix..." << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;

        // Connect request completed signal to member function
        camera_->requestCompleted.connect(this, &CameraToMatrix::processRequest);

        // Keep running until interrupted
        while (running) {
            usleep(10000); // 10ms sleep
        }

        // Cleanup
        camera_->stop();
        delete allocator;
    }

    void processRequest(Request *request) {
        if (!running) return;
        
        // Process frame
        const Request::BufferMap &buffers = request->buffers();
        for (auto it = buffers.begin(); it != buffers.end(); ++it) {
            FrameBuffer *buffer = it->second;
            const FrameMetadata &metadata = buffer->metadata();

            if (metadata.status == FrameMetadata::FrameSuccess) {
                // Get frame data
                const FrameBuffer::Plane &plane = buffer->planes()[0];
                void *data = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
                
                if (data != MAP_FAILED) {
                    // Convert and display on matrix
                    displayFrame(static_cast<uint8_t*>(data), width_, height_);
                    munmap(data, plane.length);
                }
            }
        }

        // Re-queue request
        request->reuse(Request::ReuseBuffers);
        camera_->queueRequest(request);
    }

private:
    void setupMatrix() {
        std::cerr << "=== Setting up matrix (UID: " << geteuid() << ") ===" << std::endl;
        
        RGBMatrix::Options options;
        options.rows = rows_;
        options.cols = cols_;
        options.chain_length = chain_length_;
        options.parallel = parallel_;
        options.hardware_mapping = hardware_mapping_.c_str();
        options.brightness = 50;
        
        RuntimeOptions runtime_options;
        // Don't let RGB matrix library drop privileges - we'll do it manually
        // This gives us more control over when and to whom we drop
        runtime_options.drop_privileges = 0;
        
        std::cerr << "Creating RGB matrix..." << std::endl;
        matrix_ = RGBMatrix::CreateFromOptions(options, runtime_options);
        if (!matrix_) {
            std::cerr << "ERROR: Failed to create RGB matrix" << std::endl;
        } else {
            canvas_ = matrix_->CreateFrameCanvas();
            std::cerr << "Matrix created successfully" << std::endl;
            
            // Don't drop privileges here - wait until after camera is acquired
            std::cerr << "Matrix created (UID: " << geteuid() << ")" << std::endl;
        }
    }

    void setupCameraManager() {
        // Setup camera manager while still root (needed for DMA buffer access)
        std::cerr << "Setting up camera manager (UID: " << geteuid() << ")..." << std::endl;
        camera_manager_ = std::make_unique<CameraManager>();
        if (camera_manager_->start()) {
            std::cerr << "Failed to start camera manager" << std::endl;
            return;
        }
        std::cerr << "Camera manager started successfully" << std::endl;
    }

    void acquireCamera() {
        std::cerr << "Acquiring camera (UID: " << geteuid() << ")..." << std::endl;
        
        if (!camera_manager_) {
            std::cerr << "Camera manager not initialized" << std::endl;
            return;
        }

        std::vector<std::shared_ptr<Camera>> cameras = camera_manager_->cameras();
        if (cameras.empty()) {
            std::cerr << "No cameras found" << std::endl;
            return;
        }

        camera_ = cameras[0];
        if (camera_->acquire()) {
            std::cerr << "Failed to acquire camera" << std::endl;
            camera_.reset();
        } else {
            std::cerr << "Camera acquired successfully" << std::endl;
        }
    }

    void displayFrame(uint8_t *data, int width, int height) {
        if (!canvas_) return;

        // Get matrix dimensions
        int matrix_width = canvas_->width();
        int matrix_height = canvas_->height();

        // Resize and convert frame to matrix size
        for (int y = 0; y < matrix_height; y++) {
            for (int x = 0; x < matrix_width; x++) {
                // Scale coordinates
                int src_x = (x * width) / matrix_width;
                int src_y = (y * height) / matrix_height;
                
                // Get pixel from camera frame (RGB888 format)
                int src_idx = (src_y * width + src_x) * 3;
                uint8_t r = data[src_idx];
                uint8_t g = data[src_idx + 1];
                uint8_t b = data[src_idx + 2];

                // Set pixel on matrix
                canvas_->SetPixel(x, y, r, g, b);
            }
        }

        // Swap canvas on VSync
        canvas_ = matrix_->SwapOnVSync(canvas_);
    }

    void cleanup() {
        if (camera_) {
            camera_->release();
            camera_.reset();
        }
        if (camera_manager_) {
            camera_manager_->stop();
            camera_manager_.reset();
        }
        if (matrix_) {
            matrix_->Clear();
        }
    }

    int width_;
    int height_;
    int rows_;
    int cols_;
    int chain_length_;
    int parallel_;
    std::string hardware_mapping_;
    RGBMatrix *matrix_;
    FrameCanvas *canvas_;
    std::unique_ptr<CameraManager> camera_manager_;
    std::shared_ptr<Camera> camera_;
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
