#pragma once

#include <pulse/simple.h>
#include <string>
#include <vector>
#include <atomic>


// VirtualMic creates a PulseAudio source that other apps can use as input.
class VirtualMic {
public:
    VirtualMic();
    ~VirtualMic();

    bool initialize();
    void write(const std::vector<int16_t>& samples);
    void cleanup();
    
    bool isActive() const { return active_; }
    
    static bool createVirtualDevice();
    static void removeVirtualDevice();
    static bool isVirtualDeviceLoaded();

private:
    pa_simple* stream_ = nullptr;
    std::atomic<bool> active_{false};
    
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 1;
};
