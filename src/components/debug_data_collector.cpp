#include "components/debug_data_collector.h"
#include <fstream>

DebugDataCollector::DebugDataCollector()
    : frame_count_(0),
      last_fps_time_(std::chrono::steady_clock::now()),
      current_fps_(0.0) {
}

void DebugDataCollector::recordFrame() {
    frame_count_++;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time_).count();
    
    if (elapsed >= 1000) {  // Update FPS every second
        current_fps_ = (frame_count_ * 1000.0) / elapsed;
        frame_count_ = 0;
        last_fps_time_ = now;
    }
}

double DebugDataCollector::getFPS() const {
    return current_fps_.load();
}

float DebugDataCollector::getTemperature() const {
    return readTemperature();
}

float DebugDataCollector::readTemperature() const {
    std::ifstream temp_file("/sys/class/thermal/thermal_zone0/temp");
    if (temp_file.is_open()) {
        int temp_millidegrees;
        temp_file >> temp_millidegrees;
        temp_file.close();
        return temp_millidegrees / 1000.0f;  // Convert from millidegrees to degrees
    }
    return 0.0f;  // Return 0 if unable to read
}
