#include "components/camera_capture.h"
#include <iostream>
#include <array>
#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>

// External running flag (defined in main)
extern volatile bool running;

CameraCapture::CameraCapture(int width, int height, int sensor_width, int sensor_height) 
    : width_(width), height_(height), 
      sensor_width_(sensor_width), sensor_height_(sensor_height),
      actual_width_(0), actual_height_(0),
      allocator_(nullptr), frame_callback_connected_(false) {
    setup();
}

CameraCapture::~CameraCapture() {
    cleanup();
}

bool CameraCapture::isReady() const {
    return camera_ != nullptr;
}

int CameraCapture::getWidth() const { 
    return width_; 
}

int CameraCapture::getHeight() const { 
    return height_; 
}

void CameraCapture::start() {
    if (!camera_) {
        std::cerr << "Camera not available" << std::endl;
        return;
    }

    // Configure camera stream
    // Use VideoRecording role to bias libcamera toward high-throughput/high-fps pipelines.
    // std::unique_ptr<CameraConfiguration> config = camera_->generateConfiguration({StreamRole::VideoRecording});

    std::unique_ptr<CameraConfiguration> config = camera_->generateConfiguration({StreamRole::Viewfinder});
    if (!config) {
        std::cerr << "Failed to generate camera configuration" << std::endl;
        return;
    }

    // Set stream format
    StreamConfiguration &streamConfig = config->at(0);
    
    // Always configure stream at the desired OUTPUT resolution
    streamConfig.size.width = width_;
    streamConfig.size.height = height_;
    streamConfig.pixelFormat = formats::RGB888;
    streamConfig.bufferCount = 6;  // More buffers helps sustain higher FPS
    
    std::cout << "Requesting output stream: " << width_ << "x" << height_ << " (RGB888)" << std::endl;
    if (sensor_width_ > 0 && sensor_height_ > 0) {
        std::cout << "Will request sensor mode ~" << sensor_width_ << "x" << sensor_height_ 
                  << " via ScalerCrop for FOV control" << std::endl;
    }
    std::cout << "Requesting ~120fps via FrameDurationLimits (8333us)" << std::endl;
    
    config->validate();

    if (camera_->configure(config.get()) < 0) {
        std::cerr << "Failed to configure camera" << std::endl;
        return;
    }

    // Store actual stream dimensions after configure (may differ after validate/configure)
    actual_width_ = config->at(0).size.width;
    actual_height_ = config->at(0).size.height;
    std::cout << "Actual configured stream: " << actual_width_ << "x" << actual_height_ << std::endl;

    // Allocate buffers
    allocator_ = new FrameBufferAllocator(camera_);
    for (StreamConfiguration &cfg : *config) {
        int ret = allocator_->allocate(cfg.stream());
        if (ret < 0) {
            std::cerr << "Failed to allocate buffers" << std::endl;
            return;
        }
    }

    // Start camera
    if (camera_->start()) {
        std::cerr << "Failed to start camera" << std::endl;
        return;
    }

    // Use ScalerCrop to control FOV
    // ScalerCrop specifies which part of the sensor's active area to use
    // FULL sensor = widest FOV, cropped = narrower FOV
    Rectangle scaler_crop;
    bool use_scaler_crop = false;
    
    if (sensor_width_ > 0 && sensor_height_ > 0) {
        const auto &sensorSize = camera_->properties().get(properties::PixelArraySize);
        if (sensorSize) {
            Size sensor_full = *sensorSize;
            
            // Use the FULL sensor active area for widest FOV
            // Libcamera will pick a sensor mode that covers this area and scale down
            scaler_crop = Rectangle(0, 0, sensor_full.width, sensor_full.height);
            use_scaler_crop = true;
            std::cout << "Setting ScalerCrop to FULL sensor: " << scaler_crop.toString() 
                      << " for maximum FOV" << std::endl;
        }
    }

    // Create and queue requests
    for (const StreamConfiguration &cfg : *config) {
        const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator_->buffers(cfg.stream());
        for (unsigned int i = 0; i < buffers.size(); ++i) {
            std::unique_ptr<Request> request = camera_->createRequest();
            if (!request) {
                std::cerr << "Can't create request" << std::endl;
                return;
            }

            const std::unique_ptr<FrameBuffer> &buffer = buffers[i];
            int ret = request->addBuffer(cfg.stream(), buffer.get());
            if (ret < 0) {
                std::cerr << "Can't set buffer for request" << std::endl;
                return;
            }

            // Request high FPS by constraining frame duration.
            // FrameDurationLimits is in microseconds. 120 fps -> 8333 us.
            // Note: If exposure exceeds this, the camera may still not reach 120 fps in low light.
            const std::array<int64_t, 2> frame_duration_limits = {8333, 8333};
            request->controls().set(controls::FrameDurationLimits, frame_duration_limits);
            
            // Set ScalerCrop to control sensor mode / FOV
            if (use_scaler_crop) {
                request->controls().set(controls::ScalerCrop, scaler_crop);
            }

            camera_->queueRequest(request.release());
        }
    }
}

void CameraCapture::stop() {
    if (camera_) {
        camera_->stop();
    }
}

void CameraCapture::setFrameCallback(std::function<void(uint8_t*, int, int)> callback) {
    frame_callback_ = callback;
    // Connect callback signal if camera is available
    if (camera_ && !frame_callback_connected_) {
        camera_->requestCompleted.connect(this, &CameraCapture::processRequest);
        frame_callback_connected_ = true;
    }
}

void CameraCapture::setup() {
    camera_manager_ = std::make_unique<CameraManager>();
    camera_manager_->start();

    // Acquire the first available camera
    std::vector<std::shared_ptr<Camera>> cameras = camera_manager_->cameras();
    if (!cameras.empty()) {
        camera_ = cameras[0];
        if (camera_->acquire()) {
            camera_.reset();
        }
    }
}

void CameraCapture::processRequest(Request *request) {
    if (!running || !frame_callback_) return;
    
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
                // Call the frame callback with actual camera dimensions
                frame_callback_(static_cast<uint8_t*>(data), actual_width_, actual_height_);
                munmap(data, plane.length);
            }
        }
    }

    // Re-queue request
    request->reuse(Request::ReuseBuffers);

    camera_->queueRequest(request);
}

void CameraCapture::cleanup() {
    if (camera_) {
        camera_->stop();
        camera_->release();
        camera_.reset();
    }
    if (allocator_) {
        delete allocator_;
        allocator_ = nullptr;
    }
    if (camera_manager_) {
        camera_manager_->stop();
        camera_manager_.reset();
    }
}
