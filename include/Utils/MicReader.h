#pragma once
#include <soundio/soundio.h>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>

using AudioCallback = std::function<std::vector<int16_t>(const std::vector<int16_t>&)>;

class MicrophoneReader {
public:
    MicrophoneReader();
    ~MicrophoneReader();

    std::vector<std::string> listDevices();
    std::vector<std::string> listPlaybackDevices();
    bool selectDevice(const std::string& display_name);
    bool selectPlaybackDevice(const std::string& display_name);
    void setMonitorEnabled(bool enabled);
    void setAudioCallback(AudioCallback callback);
    bool initialize();
    void processAudio();
    void cleanup();

private:
    SoundIo* soundio_;
    SoundIoDevice* input_device_;
    SoundIoDevice* output_device_;
    SoundIoInStream* instream_;
    SoundIoOutStream* outstream_;
    
    std::string selected_device_;
    std::string selected_playback_device_;
    int selected_device_index_;
    int selected_playback_index_;
    
    std::map<std::string, int> mic_name_map_;
    std::map<std::string, int> speaker_name_map_;
    
    bool monitor_enabled_;
    AudioCallback audio_callback_;
    
    static const unsigned int sample_rate_ = 48000;
    static const unsigned int channels_ = 1;
    static const int frame_size_ = 480;
    
    // Ring buffer for output
    std::vector<int16_t> ring_buffer_;
    std::atomic<size_t> write_pos_;
    std::atomic<size_t> read_pos_;
    std::atomic<size_t> samples_available_;
    std::atomic<bool> running_;
    
    // Processing buffer to accumulate samples for exact 480-sample chunks
    std::vector<int16_t> process_buffer_;
    
    static void readCallback(SoundIoInStream* instream, int frame_count_min, int frame_count_max);
    static void writeCallback(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max);
    static void underflowCallback(SoundIoOutStream* outstream);
    static void overflowCallback(SoundIoInStream* instream);
};