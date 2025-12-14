#include "Utils/MicReader.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <csignal>
#include <cmath>

static volatile bool keep_running = true;

void signal_handler(int) {
    keep_running = false;
}

MicrophoneReader::MicrophoneReader()
    : capture_handle_(nullptr),
      playback_handle_(nullptr),
      monitor_enabled_(false),
      audio_callback_(nullptr) {
    std::signal(SIGINT, signal_handler);
}

MicrophoneReader::~MicrophoneReader() {
    cleanup();
}

bool MicrophoneReader::shouldIncludeDevice(const char* name) {
    if (!name) return false;
    std::string device(name);
    return device == "default" || device.find("hw:") == 0 || device.find("plughw:") == 0;
}

std::vector<std::string> MicrophoneReader::listDevices() {
    std::vector<std::string> display_names;
    mic_name_map_.clear();
    void** hints;

    if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
        return display_names;
    }

    void** hint = hints;
    while (*hint != nullptr) {
        char* name = snd_device_name_get_hint(*hint, "NAME");
        char* desc = snd_device_name_get_hint(*hint, "DESC");
        char* ioid = snd_device_name_get_hint(*hint, "IOID");

        if (shouldIncludeDevice(name) && (ioid == nullptr || strcmp(ioid, "Input") == 0)) {
            std::string actual_name = name;
            std::string display_name;
            
            if (strcmp(name, "default") == 0) {
                display_name = "Default Microphone";
            } else if (desc) {
                display_name = desc;
                size_t newline = display_name.find('\n');
                if (newline != std::string::npos) {
                    display_name = display_name.substr(0, newline);
                }
            } else {
                display_name = name;
            }
            
            // Check if display name already exists
            if (mic_name_map_.find(display_name) == mic_name_map_.end()) {
                display_names.push_back(display_name);
                mic_name_map_[display_name] = actual_name;
            }
        }

        if (name) free(name);
        if (desc) free(desc);
        if (ioid) free(ioid);
        hint++;
    }

    snd_device_name_free_hint(hints);
    return display_names;
}

std::vector<std::string> MicrophoneReader::listPlaybackDevices() {
   std::vector<std::string> display_names;
    speaker_name_map_.clear();
    void** hints;

    if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
        return display_names;
    }

    void** hint = hints;
    while (*hint != nullptr) {
        char* name = snd_device_name_get_hint(*hint, "NAME");
        char* desc = snd_device_name_get_hint(*hint, "DESC");
        char* ioid = snd_device_name_get_hint(*hint, "IOID");

        if (shouldIncludeDevice(name) && (ioid == nullptr || strcmp(ioid, "Output") == 0)) {
            std::string actual_name = name;
            std::string display_name;
            
            if (strcmp(name, "default") == 0) {
                display_name = "Default Speakers";
            } else if (desc) {
                display_name = desc;
                size_t newline = display_name.find('\n');
                if (newline != std::string::npos) {
                    display_name = display_name.substr(0, newline);
                }
            } else {
                display_name = name;
            }
            
            // Check if display name already exists
            if (speaker_name_map_.find(display_name) == speaker_name_map_.end()) {
                display_names.push_back(display_name);
                speaker_name_map_[display_name] = actual_name;
            }
        }

        if (name) free(name);
        if (desc) free(desc);
        if (ioid) free(ioid);
        hint++;
    }

    snd_device_name_free_hint(hints);
    return display_names;
}

bool MicrophoneReader::selectDevice(const std::string& display_name) {
    auto it = mic_name_map_.find(display_name);
    if (it != mic_name_map_.end()) {
        selected_device_ = it->second;
        std::cout << "Selected mic: " << display_name << " (" << selected_device_ << ")\n";
        return true;
    }
    std::cerr << "Device not found: " << display_name << "\n";
    return false;
}

bool MicrophoneReader::selectPlaybackDevice(const std::string& display_name) {
    auto it = speaker_name_map_.find(display_name);
    if (it != speaker_name_map_.end()) {
        selected_playback_device_ = it->second;
        std::cout << "Selected speaker: " << display_name << " (" << selected_playback_device_ << ")\n";
        return true;
    }
    std::cerr << "Device not found: " << display_name << "\n";
    return false;
}

void MicrophoneReader::setMonitorEnabled(bool enabled) {
    monitor_enabled_ = enabled;
}

void MicrophoneReader::setAudioCallback(AudioCallback callback) {
    audio_callback_ = callback;
}

bool MicrophoneReader::setupDevice(snd_pcm_t** handle, const std::string& device, snd_pcm_stream_t stream) {
    int err = snd_pcm_open(handle, device.c_str(), stream, 0);
    if (err < 0) {
        std::cerr << "Cannot open device " << device << ": " << snd_strerror(err) << "\n";
        return false;
    }

    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(*handle, hw_params);
    snd_pcm_hw_params_set_access(*handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(*handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(*handle, hw_params, channels_);
    snd_pcm_hw_params_set_rate_near(*handle, hw_params, const_cast<unsigned int*>(&sample_rate_), 0);
    
    snd_pcm_uframes_t buffer_size = frame_size_ * 8;
    snd_pcm_hw_params_set_buffer_size_near(*handle, hw_params, &buffer_size);
    
    snd_pcm_uframes_t period_size = frame_size_;
    snd_pcm_hw_params_set_period_size_near(*handle, hw_params, &period_size, 0);

    err = snd_pcm_hw_params(*handle, hw_params);
    if (err < 0) {
        std::cerr << "Cannot set hw parameters: " << snd_strerror(err) << "\n";
        return false;
    }

    snd_pcm_sw_params_t* sw_params;
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(*handle, sw_params);
    snd_pcm_sw_params_set_start_threshold(*handle, sw_params, buffer_size / 2);
    snd_pcm_sw_params_set_avail_min(*handle, sw_params, period_size);
    snd_pcm_sw_params(*handle, sw_params);

    return true;
}

bool MicrophoneReader::initialize() {
    if (selected_device_.empty()) {
        std::cerr << "No device selected\n";
        return false;
    }

    if (!setupDevice(&capture_handle_, selected_device_, SND_PCM_STREAM_CAPTURE)) {
        return false;
    }

    // Always open playback if monitoring is enabled OR if playback device is selected
    if (monitor_enabled_ || !selected_playback_device_.empty()) {
        std::string pb_device = selected_playback_device_.empty() ? "default" : selected_playback_device_;
        std::cout << "Opening playback device: " << pb_device << "\n";
        
        if (!setupDevice(&playback_handle_, pb_device, SND_PCM_STREAM_PLAYBACK)) {
            std::cerr << "✗ Failed to open playback device: " << pb_device << "\n";
            playback_handle_ = nullptr;
        } else {
            std::cout << "✓ Playback device opened successfully\n";
            // Prepare the playback device
            snd_pcm_prepare(playback_handle_);
        }
    }

    std::cout << "\n Audio initialized:\n";
    std::cout << "  Sample rate: " << sample_rate_ << " Hz\n";
    std::cout << "  Channels: " << channels_ << "\n";
    std::cout << "  Frame size: " << frame_size_ << " samples\n";
    std::cout << "  Monitoring: " << (monitor_enabled_ ? "ENABLED" : "DISABLED") << "\n";
    
    return true;
}

void MicrophoneReader::processAudio() {
    std::vector<int16_t> buffer(frame_size_);
    int frame_count = 0;
    int underrun_count = 0;
    int playback_success_count = 0;
    int playback_error_count = 0;

    std::cout << "Processing audio...\n";
    std::cout << "Playback handle: " << (playback_handle_ ? "VALID" : "NULL") << "\n";

    while (keep_running) {
        int frames_read = snd_pcm_readi(capture_handle_, buffer.data(), frame_size_);
        
        if (frames_read == -EPIPE) {
            underrun_count++;
            if (underrun_count % 10 == 1) {
                std::cerr << "Buffer underrun #" << underrun_count << "\n";
            }
            snd_pcm_prepare(capture_handle_);
            continue;
        } else if (frames_read < 0) {
            snd_pcm_recover(capture_handle_, frames_read, 1);
            continue;
        }

        if (frames_read != frame_size_) {
            continue;
        }

        // Apply callback if set
        std::vector<int16_t> output_buffer = buffer;
        if (audio_callback_) {
            try {
                output_buffer = audio_callback_(buffer);
            } catch (const std::exception& e) {
                std::cerr << "\nCallback error: " << e.what() << "\n";
                output_buffer = buffer;
            }
        }

        // DEBUG: Check signal level
        if (frame_count % 100 == 0) {
            float max_val = 0;
            for (auto s : output_buffer) {
                max_val = std::max(max_val, static_cast<float>(std::abs(s)));
            }
            std::cout << "\nOutput buffer size: " << output_buffer.size() 
                      << ", peak: " << max_val << "/" << 32768.0f << "\n";
        }

        // Monitor output - play back if we have a playback handle
        if (playback_handle_) {
            // Check playback state
            snd_pcm_state_t state = snd_pcm_state(playback_handle_);
            if (state != SND_PCM_STATE_RUNNING && state != SND_PCM_STATE_PREPARED) {
                std::cout << "Playback state: " << snd_pcm_state_name(state) << ", preparing...\n";
                snd_pcm_prepare(playback_handle_);
            }

            int frames_written = snd_pcm_writei(playback_handle_, output_buffer.data(), output_buffer.size());
            
            if (frames_written == -EPIPE) {
                std::cerr << "Playback underrun, recovering...\n";
                snd_pcm_prepare(playback_handle_);
                frames_written = snd_pcm_writei(playback_handle_, output_buffer.data(), output_buffer.size());
            } else if (frames_written < 0) {
                playback_error_count++;
                if (playback_error_count % 10 == 1) {
                    std::cerr << "Playback error #" << playback_error_count 
                              << ": " << snd_strerror(frames_written) << "\n";
                }
                snd_pcm_recover(playback_handle_, frames_written, 1);
            } else if (frames_written != static_cast<int>(output_buffer.size())) {
                std::cerr << "Partial write: " << frames_written << "/" << output_buffer.size() << "\n";
            } else {
                playback_success_count++;
            }
        } else {
            if (frame_count % 100 == 0) {
                std::cout << "WARNING: No playback handle!\n";
            }
        }

        if (frame_count % 100 == 0) {
            std::cout << "Frame " << frame_count 
                      << " (playback OK: " << playback_success_count 
                      << ", errors: " << playback_error_count 
                      << ", underruns: " << underrun_count << ")\r" << std::flush;
        }

        frame_count++;
    }

    std::cout << "\nProcessed " << frame_count << " frames\n";
    std::cout << "Playback: " << playback_success_count << " successful, " 
              << playback_error_count << " errors\n";
}

void MicrophoneReader::cleanup() {
    if (capture_handle_) {
        snd_pcm_close(capture_handle_);
        capture_handle_ = nullptr;
    }
    if (playback_handle_) {
        snd_pcm_close(playback_handle_);
        playback_handle_ = nullptr;
    }
}