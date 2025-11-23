#ifndef MIC_READER_H
#define MIC_READER_H

#include <alsa/asoundlib.h>
#include <vector>

class MicrophoneReader {
private:
    snd_pcm_t* capture_handle;
    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 1;
    const int FRAMES = 128;
    
    double calculateRMS(const std::vector<short>& buffer, int frames);
    void displayVolumeBar(int volume);
    
public:
    MicrophoneReader();
    ~MicrophoneReader();
    
    bool initialize();
    void processAudio();
    void cleanup();
};

#endif