#include "Utils/MicReader.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <csignal>
#include <cmath>
#include <thread>
#include <chrono>

static volatile bool keep_running = true;

void signal_handler(int) {
    keep_running = false;
}

MicrophoneReader::MicrophoneReader()
    : soundio_(nullptr),
      input_device_(nullptr),
      output_device_(nullptr),
      instream_(nullptr),
      outstream_(nullptr),
      selected_device_index_(-1),
      selected_playback_index_(-1),
      monitor_enabled_(false),
      audio_callback_(nullptr),
      write_pos_(0),
      read_pos_(0),
      samples_available_(0),
      running_(false) {
    
    std::signal(SIGINT, signal_handler);
    keep_running = true;
    
    // Larger ring buffer for stability - 200ms
    ring_buffer_.resize(9600, 0);
    
    soundio_ = soundio_create();
    if (!soundio_) {
        std::cerr << "Failed to create SoundIo context\n";
        return;
    }
    
    int err = soundio_connect(soundio_);
    if (err) {
        std::cerr << "Failed to connect to sound backend: " << soundio_strerror(err) << "\n";
        soundio_destroy(soundio_);
        soundio_ = nullptr;
        return;
    }
    
    soundio_flush_events(soundio_);
    std::cout << "✓ SoundIo initialized with backend: " << soundio_backend_name(soundio_->current_backend) << "\n";
}

MicrophoneReader::~MicrophoneReader() {
    cleanup();
}

std::vector<std::string> MicrophoneReader::listDevices() {
    std::vector<std::string> display_names;
    mic_name_map_.clear();
    
    if (!soundio_) {
        std::cerr << "SoundIo not initialized\n";
        return display_names;
    }
    
    soundio_flush_events(soundio_);
    
    int input_count = soundio_input_device_count(soundio_);
    int default_input = soundio_default_input_device_index(soundio_);
    
    std::cout << "Found " << input_count << " input devices\n";
    
    for (int i = 0; i < input_count; i++) {
        SoundIoDevice* device = soundio_get_input_device(soundio_, i);
        if (!device) continue;
        
        if (device->probe_error) {
            soundio_device_unref(device);
            continue;
        }
        
        std::string display_name = device->name;
        if (i == default_input) {
            display_name = "[Default] " + display_name;
        }
        
        if (mic_name_map_.find(display_name) == mic_name_map_.end()) {
            display_names.push_back(display_name);
            mic_name_map_[display_name] = i;
        }
        
        soundio_device_unref(device);
    }
    
    return display_names;
}

std::vector<std::string> MicrophoneReader::listPlaybackDevices() {
    std::vector<std::string> display_names;
    speaker_name_map_.clear();
    
    if (!soundio_) {
        std::cerr << "SoundIo not initialized\n";
        return display_names;
    }
    
    soundio_flush_events(soundio_);
    
    int output_count = soundio_output_device_count(soundio_);
    int default_output = soundio_default_output_device_index(soundio_);
    
    std::cout << "Found " << output_count << " output devices\n";
    
    for (int i = 0; i < output_count; i++) {
        SoundIoDevice* device = soundio_get_output_device(soundio_, i);
        if (!device) continue;
        
        if (device->probe_error) {
            soundio_device_unref(device);
            continue;
        }
        
        std::string display_name = device->name;
        if (i == default_output) {
            display_name = "[Default] " + display_name;
        }
        
        if (speaker_name_map_.find(display_name) == speaker_name_map_.end()) {
            display_names.push_back(display_name);
            speaker_name_map_[display_name] = i;
        }
        
        soundio_device_unref(device);
    }
    
    return display_names;
}

bool MicrophoneReader::selectDevice(const std::string& display_name) {
    auto it = mic_name_map_.find(display_name);
    if (it != mic_name_map_.end()) {
        selected_device_ = display_name;
        selected_device_index_ = it->second;
        std::cout << "Selected mic: " << display_name << " (index " << selected_device_index_ << ")\n";
        return true;
    }
    std::cerr << "Device not found: " << display_name << "\n";
    return false;
}

bool MicrophoneReader::selectPlaybackDevice(const std::string& display_name) {
    auto it = speaker_name_map_.find(display_name);
    if (it != speaker_name_map_.end()) {
        selected_playback_device_ = display_name;
        selected_playback_index_ = it->second;
        std::cout << "Selected speaker: " << display_name << " (index " << selected_playback_index_ << ")\n";
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

void MicrophoneReader::readCallback(SoundIoInStream* instream, int frame_count_min, int frame_count_max) {
    MicrophoneReader* self = static_cast<MicrophoneReader*>(instream->userdata);
    if (!self || !self->running_) return;
    
    const int FRAME_SIZE = 480;
    int frames_left = frame_count_max;
    
    while (frames_left > 0) {
        int frame_count = frames_left;
        SoundIoChannelArea* areas;
        
        int err = soundio_instream_begin_read(instream, &areas, &frame_count);
        if (err) {
            std::cerr << "Input read error: " << soundio_strerror(err) << "\n";
            return;
        }
        
        if (frame_count == 0) break;
        
        // Accumulate samples into processing buffer
        if (areas) {
            for (int frame = 0; frame < frame_count; frame++) {
                float sample = *reinterpret_cast<float*>(areas[0].ptr + frame * areas[0].step);
                int16_t int_sample = static_cast<int16_t>(std::clamp(sample * 32767.0f, -32768.0f, 32767.0f));
                self->process_buffer_.push_back(int_sample);
            }
        }
        
        soundio_instream_end_read(instream);
        frames_left -= frame_count;
    }
    
    // Process complete 480-sample frames
    while (self->process_buffer_.size() >= FRAME_SIZE) {
        // Extract exactly FRAME_SIZE samples
        std::vector<int16_t> chunk(self->process_buffer_.begin(), 
                                    self->process_buffer_.begin() + FRAME_SIZE);
        self->process_buffer_.erase(self->process_buffer_.begin(), 
                                     self->process_buffer_.begin() + FRAME_SIZE);
        
        // Apply denoiser callback
        std::vector<int16_t> processed;
        if (self->audio_callback_) {
            try {
                processed = self->audio_callback_(chunk);
            } catch (const std::exception& e) {
                std::cerr << "Callback error: " << e.what() << "\n";
                processed = chunk;
            }
        } else {
            processed = chunk;
        }
        
        // Write to ring buffer
        if (self->monitor_enabled_ || self->outstream_) {
            size_t buffer_size = self->ring_buffer_.size();
            size_t write_pos = self->write_pos_.load();
            
            for (size_t i = 0; i < processed.size(); i++) {
                self->ring_buffer_[write_pos] = processed[i];
                write_pos = (write_pos + 1) % buffer_size;
            }
            
            self->write_pos_.store(write_pos);
            self->samples_available_.fetch_add(processed.size());
            
            // Prevent overflow
            size_t max_available = buffer_size - FRAME_SIZE;
            if (self->samples_available_.load() > max_available) {
                size_t excess = self->samples_available_.load() - max_available;
                self->read_pos_.store((self->read_pos_.load() + excess) % buffer_size);
                self->samples_available_.store(max_available);
            }
        }
    }
}

void MicrophoneReader::writeCallback(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max) {
    MicrophoneReader* self = static_cast<MicrophoneReader*>(outstream->userdata);
    if (!self || !self->running_) return;
    
    size_t buffer_size = self->ring_buffer_.size();
    int frames_left = frame_count_max;
    
    while (frames_left > 0) {
        int frame_count = frames_left;
        SoundIoChannelArea* areas;
        
        int err = soundio_outstream_begin_write(outstream, &areas, &frame_count);
        if (err) {
            std::cerr << "Output write error: " << soundio_strerror(err) << "\n";
            return;
        }
        
        if (frame_count == 0) break;
        
        size_t available = self->samples_available_.load();
        size_t read_pos = self->read_pos_.load();
        
        for (int frame = 0; frame < frame_count; frame++) {
            float float_sample = 0.0f;
            
            if (available > 0) {
                int16_t sample = self->ring_buffer_[read_pos];
                read_pos = (read_pos + 1) % buffer_size;
                float_sample = static_cast<float>(sample) / 32767.0f;
                available--;
            }
            
            *reinterpret_cast<float*>(areas[0].ptr + frame * areas[0].step) = float_sample;
        }
        
        self->read_pos_.store(read_pos);
        
        size_t old_available = self->samples_available_.load();
        size_t consumed = old_available - available;
        if (consumed > 0 && old_available >= consumed) {
            self->samples_available_.fetch_sub(consumed);
        }
        
        soundio_outstream_end_write(outstream);
        frames_left -= frame_count;
    }
}

void MicrophoneReader::underflowCallback(SoundIoOutStream* outstream) {
    // Silent - underflows expected during startup
}

void MicrophoneReader::overflowCallback(SoundIoInStream* instream) {
    MicrophoneReader* self = static_cast<MicrophoneReader*>(instream->userdata);
    if (self) {
        self->process_buffer_.clear();  // Clear on overflow to prevent buildup
    }
}

bool MicrophoneReader::initialize() {
    if (!soundio_) {
        std::cerr << "SoundIo not initialized\n";
        return false;
    }
    
    if (selected_device_index_ < 0) {
        std::cerr << "No input device selected\n";
        return false;
    }
    
    soundio_flush_events(soundio_);
    
    // Setup input device
    input_device_ = soundio_get_input_device(soundio_, selected_device_index_);
    if (!input_device_) {
        std::cerr << "Failed to get input device\n";
        return false;
    }
    
    if (input_device_->probe_error) {
        std::cerr << "Input device probe error: " << soundio_strerror(input_device_->probe_error) << "\n";
        return false;
    }
    
    std::cout << "✓ Input device: " << input_device_->name << "\n";
    
    // Create input stream
    instream_ = soundio_instream_create(input_device_);
    if (!instream_) {
        std::cerr << "Failed to create input stream\n";
        return false;
    }
    
    instream_->format = SoundIoFormatFloat32LE;
    instream_->sample_rate = sample_rate_;
    instream_->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
    instream_->software_latency = 0.02;  // 20ms
    instream_->read_callback = readCallback;
    instream_->overflow_callback = overflowCallback;
    instream_->userdata = this;
    
    int err = soundio_instream_open(instream_);
    if (err) {
        std::cerr << "Failed to open input stream: " << soundio_strerror(err) << "\n";
        return false;
    }
    
    std::cout << "✓ Input stream opened (latency: " << instream_->software_latency * 1000 << " ms)\n";
    
    // Setup output device
    if (monitor_enabled_ || selected_playback_index_ >= 0) {
        int output_index = selected_playback_index_ >= 0 ? 
                           selected_playback_index_ : 
                           soundio_default_output_device_index(soundio_);
        
        output_device_ = soundio_get_output_device(soundio_, output_index);
        if (!output_device_) {
            std::cerr << "Failed to get output device\n";
            return false;
        }
        
        std::cout << "✓ Output device: " << output_device_->name << "\n";
        
        outstream_ = soundio_outstream_create(output_device_);
        if (!outstream_) {
            std::cerr << "Failed to create output stream\n";
            return false;
        }
        
        outstream_->format = SoundIoFormatFloat32LE;
        outstream_->sample_rate = sample_rate_;
        outstream_->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
        outstream_->software_latency = 0.02;  // 20ms
        outstream_->write_callback = writeCallback;
        outstream_->underflow_callback = underflowCallback;
        outstream_->userdata = this;
        
        err = soundio_outstream_open(outstream_);
        if (err) {
            std::cerr << "Failed to open output stream: " << soundio_strerror(err) << "\n";
            return false;
        }
        
        std::cout << "✓ Output stream opened (latency: " << outstream_->software_latency * 1000 << " ms)\n";
    }
    
    std::cout << "\n Audio initialized:\n";
    std::cout << "  Sample rate: " << sample_rate_ << " Hz\n";
    std::cout << "  Channels: " << channels_ << "\n";
    std::cout << "  Frame size: " << frame_size_ << " samples\n";
    std::cout << "  Monitoring: " << (monitor_enabled_ ? "ENABLED" : "DISABLED") << "\n";
    
    return true;
}

void MicrophoneReader::processAudio() {
    if (!instream_) {
        std::cerr << "Input stream not initialized\n";
        return;
    }
    
    // Reset state
    running_ = true;
    keep_running = true;
    samples_available_ = 0;
    write_pos_ = 0;
    read_pos_ = 0;
    process_buffer_.clear();
    std::fill(ring_buffer_.begin(), ring_buffer_.end(), 0);
    
    int err = soundio_instream_start(instream_);
    if (err) {
        std::cerr << "Failed to start input stream: " << soundio_strerror(err) << "\n";
        running_ = false;
        return;
    }
    
    std::cout << "✓ Input stream started\n";
    
    if (outstream_) {
        err = soundio_outstream_start(outstream_);
        if (err) {
            std::cerr << "Failed to start output stream: " << soundio_strerror(err) << "\n";
        } else {
            std::cout << "✓ Output stream started\n";
        }
    }
    
    std::cout << "Processing audio... Press Ctrl+C to stop\n";
    
    while (keep_running && running_) {
        soundio_flush_events(soundio_);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    running_ = false;
    std::cout << "\nAudio processing stopped\n";
}

void MicrophoneReader::cleanup() {
    running_ = false;
    
    if (instream_) {
        soundio_instream_destroy(instream_);
        instream_ = nullptr;
    }
    
    if (outstream_) {
        soundio_outstream_destroy(outstream_);
        outstream_ = nullptr;
    }
    
    if (input_device_) {
        soundio_device_unref(input_device_);
        input_device_ = nullptr;
    }
    
    if (output_device_) {
        soundio_device_unref(output_device_);
        output_device_ = nullptr;
    }
    
    if (soundio_) {
        soundio_destroy(soundio_);
        soundio_ = nullptr;
    }
    
    process_buffer_.clear();
}