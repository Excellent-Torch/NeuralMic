#include "Utils/VirtualMic.h"
#include <pulse/error.h>
#include <iostream>
#include <cstdlib>
#include <array>
#include <memory>
#include <thread>
#include <chrono>

VirtualMic::VirtualMic() = default;

VirtualMic::~VirtualMic() {
    cleanup();
}

bool VirtualMic::createVirtualDevice() {
    if (isVirtualDeviceLoaded()) {
        std::cout << "Virtual device already exists\n";
        return true;
    }
    
    // Step 1: Create null sink to receive our audio
    int ret = std::system(
        "pactl load-module module-null-sink "
        "sink_name=NeuralMic_Sink "
        "sink_properties=device.description=NeuralMic_Sink "
        ">/dev/null 2>&1"
    );
    
    if (ret != 0) {
        std::cerr << "Failed to create null sink\n";
        return false;
    }
    
    // Small delay for PulseAudio to register the sink
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Step 2: Create virtual source (microphone) from the sink's monitor
    // This makes it appear as a real microphone input device
    ret = std::system(
        "pactl load-module module-remap-source "
        "master=NeuralMic_Sink.monitor "
        "source_name=NeuralMic "
        "source_properties=device.description=NeuralMic "
        ">/dev/null 2>&1"
    );
    
    if (ret != 0) {
        std::cerr << "Failed to create virtual microphone source\n";
        removeVirtualDevice();
        return false;
    }
    
    std::cout << "✓ Created 'NeuralMic' virtual microphone\n";
    std::cout << "  Select 'NeuralMic' as input in Your System/Application Settings\n";
    return true;
}

void VirtualMic::removeVirtualDevice() {
    std::system("pactl unload-module module-remap-source 2>/dev/null");
    std::system("pactl unload-module module-null-sink 2>/dev/null");
}

bool VirtualMic::isVirtualDeviceLoaded() {
    std::array<char, 256> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("pactl list sources short 2>/dev/null | grep -w NeuralMic", "r"), pclose);
    
    if (!pipe) return false;
    return fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr;
}

bool VirtualMic::initialize() {
    if (!createVirtualDevice()) {
        return false;
    }
    
    pa_sample_spec spec{};
    spec.format = PA_SAMPLE_S16LE;
    spec.rate = kSampleRate;
    spec.channels = kChannels;
    
    int error;
    stream_ = pa_simple_new(
        nullptr,              // Default server
        "NeuralMic",          // App name
        PA_STREAM_PLAYBACK,   // Playback to sink
        "NeuralMic_Sink",     // Sink name (the null sink)
        "Denoised Audio",     // Stream description
        &spec,
        nullptr,              // Default channel map
        nullptr,              // Default buffering
        &error
    );
    
    if (!stream_) {
        std::cerr << "PulseAudio error: " << pa_strerror(error) << "\n";
        return false;
    }
    
    active_ = true;
    std::cout << "✓ Virtual microphone active\n";
    return true;
}

void VirtualMic::write(const std::vector<int16_t>& samples) {
    if (!stream_ || samples.empty()) return;
    
    int error;
    if (pa_simple_write(stream_, samples.data(), 
                        samples.size() * sizeof(int16_t), &error) < 0) {
        std::cerr << "Write error: " << pa_strerror(error) << "\n";
    }
}

void VirtualMic::cleanup() {
    active_ = false;
    if (stream_) {
        pa_simple_drain(stream_, nullptr);
        pa_simple_free(stream_);
        stream_ = nullptr;
    }
}
