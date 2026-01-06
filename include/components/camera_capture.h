#ifndef CAMERA_CAPTURE_H
#define CAMERA_CAPTURE_H

#include <functional>
#include <memory>
#include <libcamera/libcamera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/camera.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/controls.h>

using namespace libcamera;

class CameraCapture {
public:
    CameraCapture(int width, int height);
    ~CameraCapture();

    bool isReady() const;
    int getWidth() const;
    int getHeight() const;

    void start();
    void stop();
    void setFrameCallback(std::function<void(uint8_t*, int, int)> callback);

private:
    void setup();
    void processRequest(Request *request);
    void cleanup();

    int width_;
    int height_;
    int actual_width_;   // Actual camera stream width
    int actual_height_;  // Actual camera stream height
    std::unique_ptr<CameraManager> camera_manager_;
    std::shared_ptr<Camera> camera_;
    FrameBufferAllocator *allocator_;
    std::function<void(uint8_t*, int, int)> frame_callback_;
    bool frame_callback_connected_;
};

#endif // CAMERA_CAPTURE_H
