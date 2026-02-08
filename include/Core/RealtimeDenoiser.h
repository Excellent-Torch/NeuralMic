#pragma once

#include <string>
#include <memory>
#include <vector>

class DeepFilterNet;
class MicrophoneReader;

class RealtimeDenoiser {
public:
    RealtimeDenoiser();
    ~RealtimeDenoiser();

    // Model management
    bool loadModel(const std::string& model_path);
    void setNoiseSuppressionStrength(float strength);
    
    // Device management
    std::vector<std::string> listMicrophones();
    std::vector<std::string> listSpeakers();
    bool selectMicrophone(int index);
    bool selectSpeaker(int index);
    
    // Processing control
    bool initialize();
    void start();
    void stop();
    bool isRunning() const;
    
    // Monitoring
    void enableMonitoring(bool enable);
    void enableVirtualMic(bool enable);

private:
    std::unique_ptr<DeepFilterNet> denoiser_;
    std::unique_ptr<MicrophoneReader> mic_reader_;
    
    std::vector<std::string> available_mics_;
    std::vector<std::string> available_speakers_;

    std::vector<float> frame_buffer_;  // Add this
    const size_t process_chunk_size_ = 48000;  // Process 1 second at a time
    
    bool initialized_;
    bool running_;
    bool monitoring_enabled_;
    bool virtual_mic_enabled_;
    
    // Audio processing callback
    std::vector<int16_t> processAudioFrame(const std::vector<int16_t>& input);
    
    // Conversion helpers
    std::vector<float> convertToFloat(const std::vector<int16_t>& samples);
    std::vector<int16_t> convertToInt16(const std::vector<float>& samples);
};