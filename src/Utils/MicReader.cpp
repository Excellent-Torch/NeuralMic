#include "Utils/MicReader.h"
#include <iostream>
#include <cmath>

MicrophoneReader::MicrophoneReader() : capture_handle(nullptr) {}

MicrophoneReader::~MicrophoneReader() {
    cleanup();
}

bool MicrophoneReader::initialize() {
    int err;
    
    // Open default microphone device
    err = snd_pcm_open(&capture_handle, "default", 
                      SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "Cannot open audio device: " 
                  << snd_strerror(err) << std::endl;
        return false;
    }
    
    // Set hardware parameters
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);
    
    // Configure audio format
    snd_pcm_hw_params_set_access(capture_handle, hw_params, 
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, hw_params, 
                                 SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_rate(capture_handle, hw_params, 
                               SAMPLE_RATE, 0);
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, 
                                   CHANNELS);
    
    // Apply settings
    err = snd_pcm_hw_params(capture_handle, hw_params);
    if (err < 0) {
        std::cerr << "Cannot set parameters: " 
                  << snd_strerror(err) << std::endl;
        return false;
    }
    
    snd_pcm_prepare(capture_handle);
    
    std::cout << "Microphone initialized successfully!" << std::endl;
    return true;
}

double MicrophoneReader::calculateRMS(const std::vector<short>& buffer, 
                                      int frames) {
    double sum = 0;
    for (int i = 0; i < frames; i++) {
        sum += buffer[i] * buffer[i];
    }
    return sqrt(sum / frames);
}

void MicrophoneReader::displayVolumeBar(int volume) {
    std::cout << "\rVolume: [";
    for (int i = 0; i < 50; i++) {
        std::cout << (i < volume / 2 ? "=" : " ");
    }
    std::cout << "] " << volume << "%  " << std::flush;
}

void MicrophoneReader::processAudio() {
    std::vector<short> buffer(FRAMES);

    std::cout << "Press Ctrl+C to stop." << std::endl;

    while (true) {
        // Read audio data from microphone
        int frames_read = snd_pcm_readi(capture_handle, 
                                       buffer.data(), FRAMES);
        
        if (frames_read < 0) {
            snd_pcm_prepare(capture_handle);
            continue;
        }
        
        // Calculate volume level
        double rms = calculateRMS(buffer, frames_read);
        int volume = static_cast<int>((rms / 32768.0) * 100);
        
        // Display real-time volume
        displayVolumeBar(volume);
    }

    
}

void MicrophoneReader::cleanup() {
    if (capture_handle) {
        snd_pcm_close(capture_handle);
        std::cout << "\nMicrophone closed." << std::endl;
        capture_handle = nullptr;
    }
}