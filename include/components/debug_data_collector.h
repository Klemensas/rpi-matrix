#ifndef DEBUG_DATA_COLLECTOR_H
#define DEBUG_DATA_COLLECTOR_H

#include <atomic>
#include <chrono>

class DebugDataCollector {
public:
    DebugDataCollector();
    
    // Call this for each frame to update FPS tracking
    void recordFrame();
    
    // Get current FPS (updated every second)
    double getFPS() const;
    
    // Read current temperature from system
    float getTemperature() const;

private:
    // FPS tracking
    std::atomic<uint64_t> frame_count_;
    std::chrono::steady_clock::time_point last_fps_time_;
    std::atomic<double> current_fps_;
    
    float readTemperature() const;
};

#endif // DEBUG_DATA_COLLECTOR_H
