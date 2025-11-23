#include "Utils/MicReader.h"
#include <iostream>
#include <cmath>

MicrophoneReader::MicrophoneReader() 
    : capture_handle(nullptr), device_name("default") {}

MicrophoneReader::~MicrophoneReader() {
    cleanup();
}

bool MicrophoneReader::initialize() {
    int err;
    
    // Open selected microphone device
    err = snd_pcm_open(&capture_handle, device_name.c_str(), 
                      SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "Cannot open audio device '" << device_name 
                  << "': " << snd_strerror(err) << std::endl;
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
    
    std::cout << "Microphone initialized successfully on device: " 
              << device_name << std::endl;
    return true;
}

std::vector<std::string> MicrophoneReader::listDevices() {
    std::vector<std::string> devices;
    snd_ctl_card_info_t* cardinfo;
    snd_pcm_info_t* pcminfo;
    snd_ctl_t* handle;
    int card = -1;
    
    snd_ctl_card_info_alloca(&cardinfo);
    snd_pcm_info_alloca(&pcminfo);
    
    std::cout << "\nAvailable microphone devices:\n" << std::endl;
    
    while (snd_card_next(&card) == 0 && card >= 0) {
        std::string cardname = "hw:" + std::to_string(card);
        
        if (snd_ctl_open(&handle, cardname.c_str(), 0) < 0) {
            continue;
        }
        
        if (snd_ctl_card_info(handle, cardinfo) < 0) {
            snd_ctl_close(handle);
            continue;
        }
        
        int device = -1;
        while (snd_ctl_pcm_next_device(handle, &device) == 0 && device >= 0) {
            snd_pcm_info_set_device(pcminfo, device);
            snd_pcm_info_set_subdevice(pcminfo, 0);
            snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_CAPTURE);
            
            if (snd_ctl_pcm_info(handle, pcminfo) < 0) {
                continue;
            }
            
            std::string fullname = "hw:" + std::to_string(card) + 
                                   "," + std::to_string(device);
            std::string description = std::string(snd_ctl_card_info_get_name(cardinfo)) +
                                     " - " + std::string(snd_pcm_info_get_name(pcminfo));
            
            devices.push_back(fullname);
            std::cout << "  [" << devices.size() - 1 << "] " 
                      << fullname << " - " << description << std::endl;
        }
        
        snd_ctl_close(handle);
    }
    
    // Add default device
    devices.insert(devices.begin(), "default");
    std::cout << "  [0] default - Default microphone device\n" << std::endl;
    
    return devices;
}

bool MicrophoneReader::selectDevice(const std::string& device) {
    device_name = device;
    std::cout << "Selected device: " << device_name << std::endl;
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