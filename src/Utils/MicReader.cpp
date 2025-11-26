#include "Utils/MicReader.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>

MicrophoneReader::MicrophoneReader() 
    : capture_handle(nullptr), playback_handle(nullptr),
      device_name("default"), playback_device_name("default"),
      monitorEnabled(false) {}

MicrophoneReader::~MicrophoneReader() { cleanup(); }

bool MicrophoneReader::setupPCM(snd_pcm_t*& handle, const std::string& device, 
                                 snd_pcm_stream_t stream) {
    int err = snd_pcm_open(&handle, device.c_str(), stream, 0);
    if (err < 0) {
        std::cerr << "Cannot open device '" << device << "': " << snd_strerror(err) << std::endl;
        return false;
    }
    
    err = snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, 
                             SND_PCM_ACCESS_RW_INTERLEAVED,
                             CHANNELS, SAMPLE_RATE, 1, 50000);
    if (err < 0) {
        std::cerr << "Cannot set params: " << snd_strerror(err) << std::endl;
        snd_pcm_close(handle);
        handle = nullptr;
        return false;
    }
    return true;
}

bool MicrophoneReader::initialize() {
    if (!setupPCM(capture_handle, device_name, SND_PCM_STREAM_CAPTURE))
        return false;

    if (monitorEnabled.load()) {
        if (setupPCM(playback_handle, playback_device_name, SND_PCM_STREAM_PLAYBACK)) {
            std::cout << "Playback initialized on: " << playback_device_name << std::endl;
        } else {
            playback_handle = nullptr;
        }
    }

    std::cout << "Microphone initialized on: " << device_name << std::endl;
    return true;
}

std::vector<std::string> MicrophoneReader::listDevices() {
    return listPCMDevices(SND_PCM_STREAM_CAPTURE, "microphone");
}

std::vector<std::string> MicrophoneReader::listPlaybackDevices() {
    return listPCMDevices(SND_PCM_STREAM_PLAYBACK, "playback");
}

std::vector<std::string> MicrophoneReader::listPCMDevices(snd_pcm_stream_t stream,
                                                          const std::string& type) {
    std::vector<std::string> devices = {"default"};
    snd_ctl_card_info_t* cardinfo;
    snd_pcm_info_t* pcminfo;
    snd_ctl_t* handle;
    int card = -1;
    
    snd_ctl_card_info_alloca(&cardinfo);
    snd_pcm_info_alloca(&pcminfo);
    
    std::cout << "\nAvailable " << type << " devices:\n  [0] default\n" << std::endl;
    
    while (snd_card_next(&card) == 0 && card >= 0) {
        std::string cardname = "hw:" + std::to_string(card);
        if (snd_ctl_open(&handle, cardname.c_str(), 0) < 0) continue;
        if (snd_ctl_card_info(handle, cardinfo) < 0) {
            snd_ctl_close(handle);
            continue;
        }
        
        int device = -1;
        while (snd_ctl_pcm_next_device(handle, &device) == 0 && device >= 0) {
            snd_pcm_info_set_device(pcminfo, device);
            snd_pcm_info_set_stream(pcminfo, stream);
            
            if (snd_ctl_pcm_info(handle, pcminfo) < 0) continue;
            
            std::string fullname = "hw:" + std::to_string(card) + "," + std::to_string(device);
            std::string desc = std::string(snd_ctl_card_info_get_name(cardinfo)) +
                              " - " + std::string(snd_pcm_info_get_name(pcminfo));
            
            devices.push_back(fullname);
            std::cout << "  [" << devices.size() - 1 << "] " << desc << std::endl;
        }
        snd_ctl_close(handle);
    }
    std::cout << std::endl;
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
    std::cout << "Monitor: " << (enabled ? "ON" : "OFF") << std::endl;
}

double MicrophoneReader::calculateRMS(const std::vector<short>& buffer, int num_samples) {
    if (num_samples <= 0) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < num_samples; ++i) {
        sum += static_cast<double>(buffer[i]) * buffer[i];
    }
    return std::sqrt(sum / num_samples);
}

void MicrophoneReader::displayVolumeBar(int volume) {
    std::cout << "\rVolume: [";
    for (int i = 0; i < 50; i++) 
        std::cout << (i < volume / 2 ? "=" : " ");
    std::cout << "] " << volume << "%  " << std::flush;
}

void MicrophoneReader::writeToPlayback(const short* data, int frames) {
    if (!playback_handle || frames <= 0) return;
    
    int frames_left = frames;
    const short* ptr = data;
    
    while (frames_left > 0) {
        int written = snd_pcm_writei(playback_handle, ptr, frames_left);
        
        if (written < 0) {
            if (snd_pcm_recover(playback_handle, written, 0) < 0) {
                snd_pcm_close(playback_handle);
                playback_handle = nullptr;
                return;
            }
            break;
        }
        
        if (written > 0) {
            ptr += written * CHANNELS;
            frames_left -= written;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            break;
        }
    }
}

void MicrophoneReader::processAudio() {
    std::vector<short> buffer(FRAMES * CHANNELS, 0);
    std::cout << "Press Ctrl+C to stop.\n" << std::endl;

    while (true) {
        int frames_read = snd_pcm_readi(capture_handle, buffer.data(), FRAMES);

        if (frames_read < 0) {
            if (snd_pcm_recover(capture_handle, frames_read, 0) < 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            continue;
        }
        if (frames_read == 0) continue;

        // Calculate and display volume
        double rms = calculateRMS(buffer, frames_read * CHANNELS);
        int volume = std::clamp(static_cast<int>((rms / 32767.0) * 100), 0, 100);
        displayVolumeBar(volume);

        // Write to playback if monitoring enabled
        if (monitorEnabled.load())
            writeToPlayback(buffer.data(), frames_read);
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