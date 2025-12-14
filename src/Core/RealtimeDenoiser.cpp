#include "Core/RealtimeDenoiser.h"
#include "Core/OnnxInference.h"
#include "Utils/MicReader.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <cmath>

using std::cout;
using std::cerr;
using std::string;
using std::vector;

RealtimeDenoiser::RealtimeDenoiser()
    : denoiser_(nullptr),
      mic_reader_(nullptr),
      initialized_(false),
      running_(false),
      monitoring_enabled_(false) {
}

RealtimeDenoiser::~RealtimeDenoiser() {
    stop();
}

bool RealtimeDenoiser::loadModel(const string& model_path) {
    try {
        cout << "Loading DeepFilterNet model...\n";
        denoiser_ = std::make_unique<DeepFilterNet>(model_path);
        cout << "Model loaded successfully\n";
        return true;
    } catch (const std::exception& e) {
        cerr << "Failed to load model: " << e.what() << "\n";
        return false;
    }
}

void RealtimeDenoiser::setNoiseSuppressionStrength(float strength) {
    if (!denoiser_) {
        cerr << "Model not loaded\n";
        return;
    }
    
   // Try much gentler suppression first
    float clamped = std::clamp(strength, -30.0f, 0.0f);  // Changed from -100
    denoiser_->SetNoiseSuppressionStrength(clamped);
    cout << "Noise suppression strength: " << clamped << " dB\n";
}

vector<string> RealtimeDenoiser::listMicrophones() {
    if (!mic_reader_) {
        mic_reader_ = std::make_unique<MicrophoneReader>();
    }
    available_mics_ = mic_reader_->listDevices();
    return available_mics_;
}

vector<string> RealtimeDenoiser::listSpeakers() {
    if (!mic_reader_) {
        mic_reader_ = std::make_unique<MicrophoneReader>();
    }
    available_speakers_ = mic_reader_->listPlaybackDevices();
    return available_speakers_;
}

bool RealtimeDenoiser::selectMicrophone(int index) {
    if (index < 0 || index >= static_cast<int>(available_mics_.size())) {
        cerr << "Invalid microphone index\n";
        return false;
    }
    
    if (!mic_reader_) {
        mic_reader_ = std::make_unique<MicrophoneReader>();
    }

    
    return mic_reader_->selectDevice(available_mics_[index]);
}

bool RealtimeDenoiser::selectSpeaker(int index) {
    if (index < 0 || index >= static_cast<int>(available_speakers_.size())) {
        cerr << "Invalid speaker index\n";
        return false;
    }
    
    if (!mic_reader_) {
        mic_reader_ = std::make_unique<MicrophoneReader>();
    }
    
    return mic_reader_->selectPlaybackDevice(available_speakers_[index]);
}

void RealtimeDenoiser::enableMonitoring(bool enable) {
    monitoring_enabled_ = enable;
    if (mic_reader_) {
        mic_reader_->setMonitorEnabled(enable);
    }
    cout << "Monitoring: " << (enable ? "ENABLED" : "DISABLED") << "\n";
}

vector<float> RealtimeDenoiser::convertToFloat(const vector<int16_t>& samples) {
    vector<float> result(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        result[i] = static_cast<float>(samples[i]) / 32768.0f;
    }
    return result;
}

vector<int16_t> RealtimeDenoiser::convertToInt16(const vector<float>& samples) {
    vector<int16_t> result(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        float sample = samples[i];
        
        // Handle NaN/inf
        if (std::isnan(sample) || std::isinf(sample)) {
            sample = 0.0f;
        }
        
        float scaled = sample * 32767.0f;
        int32_t clamped = std::clamp(
            static_cast<int32_t>(scaled),
            static_cast<int32_t>(-32768),
            static_cast<int32_t>(32767)
        );
        result[i] = static_cast<int16_t>(clamped);
    }
    return result;
}

vector<int16_t> RealtimeDenoiser::processAudioFrame(const vector<int16_t>& input) {
    if (!denoiser_) {
            return input;
        }
    
    // Ensure we have exactly 480 samples
    if (input.size() != 480) {
        std::cerr << "Warning: Expected 480 samples, got " << input.size() << "\n";
        return input;
    }
    
    static int frame_count = 0;
    if (frame_count++ % 100 == 0) {
        cout << "Processing frame " << frame_count << "\n";
    }
    
    // Convert to float
    auto float_samples = convertToFloat(input);
    
    // Use streaming API instead of batch processing
    auto denoised = denoiser_->ProcessRealtimeFrame(float_samples);
    
    // Convert back to int16
    return convertToInt16(denoised);
}

bool RealtimeDenoiser::initialize() {
    if (!denoiser_) {
        std::cerr << "✗ Model not loaded\n";
        return false;
    }
    
    if (!mic_reader_) {
        mic_reader_ = std::make_unique<MicrophoneReader>();
    }
    
    // Set callback to actually use the denoiser
    mic_reader_->setAudioCallback([this](const vector<int16_t>& input) {
        return this->processAudioFrame(input);
    });
    
    mic_reader_->setMonitorEnabled(monitoring_enabled_);
    
    if (!mic_reader_->initialize()) {
        std::cerr << "Failed to initialize microphone reader\n";
        return false;
    }
    
    initialized_ = true;
    cout << "Real-time denoiser initialized\n";
    return true;
}

void RealtimeDenoiser::start() {
    if (!initialized_) {
        cerr << "Not initialized. Call initialize() first.\n";
        return;
    }
    
    if (running_) {
        cerr << "Already running\n";
        return;
    }
    
    running_ = true;
    cout << "\n=== REAL-TIME NOISE SUPPRESSION ACTIVE ===\n";
    cout << "Press Ctrl+C to stop...\n\n";
    
    mic_reader_->processAudio();
    
    running_ = false;
}

void RealtimeDenoiser::stop() {
    if (running_) {
        running_ = false;
        cout << "\n=== STOPPING ===\n";
    }
    
    if (mic_reader_) {
        mic_reader_->cleanup();
    }
    
    initialized_ = false;
}

bool RealtimeDenoiser::isRunning() const {
    return running_;
}