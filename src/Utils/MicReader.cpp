#include "Utils/MicReader.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>

MicrophoneReader::MicrophoneReader() 
    : capture_handle(nullptr),
      playback_handle(nullptr),
      device_name("default"),
      playback_device_name("default"),
      monitorEnabled(false) {}

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
    
    // Use snd_pcm_set_params to set sensible, matching parameters (reduces xruns/underruns)
    err = snd_pcm_set_params(
        capture_handle,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        CHANNELS,
        SAMPLE_RATE,
        1,        // enable soft-resampling if needed
        50000     // latency in us (50 ms)
    );
    if (err < 0) {
        std::cerr << "Failed to set capture device params: " << snd_strerror(err) << std::endl;
        snd_pcm_close(capture_handle);
        capture_handle = nullptr;
        return false;
    }

    // If monitor enabled, open playback device and configure with same params
    if (monitorEnabled.load()) {
        err = snd_pcm_open(&playback_handle, playback_device_name.c_str(),
                           SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            std::cerr << "Cannot open playback device '" << playback_device_name
                      << "': " << snd_strerror(err) << std::endl;
            // Not fatal — allow operation without monitoring
            playback_handle = nullptr;
        } else {
            err = snd_pcm_set_params(
                playback_handle,
                SND_PCM_FORMAT_S16_LE,
                SND_PCM_ACCESS_RW_INTERLEAVED,
                CHANNELS,
                SAMPLE_RATE,
                1,      // soft-resample
                50000   // 50 ms latency
            );
            if (err < 0) {
                std::cerr << "Cannot set playback parameters: "
                          << snd_strerror(err) << std::endl;
                snd_pcm_close(playback_handle);
                playback_handle = nullptr;
            }
        }
    }

    std::cout << "Microphone initialized successfully on device: " 
              << device_name << std::endl;
    if (playback_handle) {
        std::cout << "Playback (monitor) initialized on: " << playback_device_name << std::endl;
    }

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
                      << description << std::endl;
        }
        
        snd_ctl_close(handle);
    }
    
    // Add default device
    devices.insert(devices.begin(), "default");
    std::cout << "  [0] default - Default microphone device\n" << std::endl;
    
    return devices;
}

std::vector<std::string> MicrophoneReader::listPlaybackDevices() {
    // Similar to listDevices but for playback stream
    std::vector<std::string> devices;
    snd_ctl_card_info_t* cardinfo;
    snd_pcm_info_t* pcminfo;
    snd_ctl_t* handle;
    int card = -1;
    
    snd_ctl_card_info_alloca(&cardinfo);
    snd_pcm_info_alloca(&pcminfo);
    
    std::cout << "\nAvailable playback devices:\n" << std::endl;
    
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
            snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);
            if (snd_ctl_pcm_info(handle, pcminfo) < 0) continue;
            std::string fullname = "hw:" + std::to_string(card) + 
                                   "," + std::to_string(device);
            std::string description = std::string(snd_ctl_card_info_get_name(cardinfo)) +
                                     " - " + std::string(snd_pcm_info_get_name(pcminfo));
            devices.push_back(fullname);
            std::cout << "  [" << devices.size() - 1 << "] " 
                      << description << std::endl;
        }
        snd_ctl_close(handle);
    }

    devices.insert(devices.begin(), "default");
    std::cout << "  [0] default - Default playback device\n" << std::endl;
    return devices;
}

bool MicrophoneReader::selectDevice(const std::string& device) {
    device_name = device;
    std::cout << "Selected device: " << device_name << std::endl;
    return true;
}

bool MicrophoneReader::selectPlaybackDevice(const std::string& device) {
    playback_device_name = device;
    std::cout << "Selected playback device: " << playback_device_name << std::endl;
    return true;
}

void MicrophoneReader::setMonitorEnabled(bool enabled) {
    monitorEnabled.store(enabled);
    std::cout << "Monitor enabled: " << (enabled ? "yes" : "no") << std::endl;
}

double MicrophoneReader::calculateRMS(const std::vector<short>& buffer, 
                                      int frames) {
    double sum = 0;
    for (int i = 0; i < frames; i++) {
        sum += buffer[i] * buffer[i];
    }
    return sqrt(sum / frames);
}   

// Display a real time volume bar in the console
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
            // recover from overrun/other errors; on success snd_pcm_recover returns >= 0 or 0.
            int r = snd_pcm_recover(capture_handle, frames_read, 0);
            if (r < 0) {
                std::cerr << "\nCapture recover failed: " << snd_strerror(r) << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // avoid busy loop
            }
            continue;
        }

        if (frames_read == 0) {
            // no frames, small sleep to avoid tight loop
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        
        // Calculate volume level (account for channels)
        double rms = calculateRMS(buffer, frames_read * CHANNELS);
        int volume = static_cast<int>((rms / 32768.0) * 100);
        displayVolumeBar(volume);

       // If monitoring enabled and playback opened, write to playback device
        if (monitorEnabled.load() && playback_handle) {
            int frames_to_write = frames_read;
            short* write_ptr = buffer.data();
            // attempt to write all frames, handle short writes
            while (frames_to_write > 0) {
                
                int written = snd_pcm_writei(playback_handle, write_ptr, frames_to_write);
                if (written < 0) {
                    int r = snd_pcm_recover(playback_handle, written, 0);
                    if (r < 0) {
                        std::cerr << "\nPlayback recover failed: " << snd_strerror(r) << std::endl;
                        // close playback to avoid continuous errors
                        snd_pcm_close(playback_handle);
                        playback_handle = nullptr;
                        break;
                    } else {
                        // recovered, break to next loop iteration to avoid immediate tight loop
                        break;
                    }
                }
                // advance buffer pointer by written frames
                if (written > 0) {
                    write_ptr += written * CHANNELS;
                    frames_to_write -= written;
                }
                
            }
        }
    }
}

void MicrophoneReader::cleanup() {
    if (capture_handle) {
        snd_pcm_close(capture_handle);
        std::cout << "\nMicrophone closed." << std::endl;
        capture_handle = nullptr;
    }
    if (playback_handle) {
        snd_pcm_close(playback_handle);
        std::cout << "Playback closed." << std::endl;
        playback_handle = nullptr;
    }
}