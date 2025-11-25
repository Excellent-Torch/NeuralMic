#ifndef MIC_READER_H
#define MIC_READER_H

#include <alsa/asoundlib.h>
#include <vector>
#include <string>
#include <atomic>

class MicrophoneReader {
private:
    snd_pcm_t* capture_handle;
    snd_pcm_t* playback_handle;

    std::string device_name;
    std::string playback_device_name; 

    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 1;
    const int FRAMES = 512;
    
    double calculateRMS(const std::vector<short>& buffer, int frames);
    void displayVolumeBar(int volume);
    
public:
    MicrophoneReader();
    ~MicrophoneReader();
    
    std::vector<std::string> listDevices();
    std::vector<std::string> listPlaybackDevices();

    bool selectDevice(const std::string& device);
    bool selectPlaybackDevice(const std::string& device);
    void setMonitorEnabled(bool enabled);
    bool initialize();
    void processAudio();
    void cleanup();

private:
    std::atomic<bool> monitorEnabled;
};

#endif