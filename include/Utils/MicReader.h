#pragma once
#include <alsa/asoundlib.h>
#include <string>
#include <vector>
#include <functional>
#include <map>

// Audio processing callback type
using AudioCallback = std::function<std::vector<int16_t>(const std::vector<int16_t>&)>;

class MicrophoneReader {
public:
    MicrophoneReader();
    ~MicrophoneReader();

    // Device discovery
    std::vector<std::string> listDevices();
    std::vector<std::string> listPlaybackDevices();
    
    // Device selection
    bool selectDevice(const std::string& display_name);
    bool selectPlaybackDevice(const std::string& display_name);
    
    // Configuration
    void setMonitorEnabled(bool enabled);
    void setAudioCallback(AudioCallback callback);
    
    // Audio I/O
    bool initialize();
    void processAudio();
    void cleanup();

private:
    snd_pcm_t* capture_handle_;
    snd_pcm_t* playback_handle_;
    
    std::string selected_device_;
    std::string selected_playback_device_;
    
    std::map<std::string, std::string> mic_name_map_;
    std::map<std::string, std::string> speaker_name_map_;
    
    bool monitor_enabled_;
    AudioCallback audio_callback_;
    
    const unsigned int sample_rate_ = 48000;
    const unsigned int channels_ = 1;
    const int frame_size_ = 480;
    
    bool setupDevice(snd_pcm_t** handle, const std::string& device, snd_pcm_stream_t stream);
    bool shouldIncludeDevice(const char* name);
};