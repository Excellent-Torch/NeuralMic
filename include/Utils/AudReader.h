// Simple Audio File Reader/Writer and Utilities
// Supports WAV and MP3 formats
// Uses minimp3 for MP3 decoding and LAME for MP3 encoding
#ifndef AUDIOFILE_H
#define AUDIOFILE_H

#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cmath>

// ============================================================================
// Audio Data Container
// ============================================================================
struct AudioFile {
    std::vector<int16_t> samples;  // Interleaved samples (L, R, L, R, ...)
    uint32_t sampleRate;           // 44100, 48000, etc.
    uint16_t channels;             // 1 = mono, 2 = stereo
    
    AudioFile() : sampleRate(44100), channels(2) {}
    
    double getDuration() const {
        if (channels == 0) return 0.0;
        return static_cast<double>(samples.size()) / (sampleRate * channels);
    }
    
    size_t getNumFrames() const {
        if (channels == 0) return 0;
        return samples.size() / channels;
    }
    
    bool isEmpty() const {
        return samples.empty();
    }
};

// ============================================================================
// WAV File Format Handler
// ============================================================================
namespace AudioIO {

// Read WAV file (robust version that handles various WAV formats)
inline bool readWav(const std::string& filename, AudioFile& audio) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    // Read RIFF header
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    file.read(riff, 4);
    file.read(reinterpret_cast<char*>(&fileSize), 4);
    file.read(wave, 4);
    
    if (std::string(riff, 4) != "RIFF" || std::string(wave, 4) != "WAVE") {
        throw std::runtime_error("Invalid WAV file");
    }
    
    // Find fmt chunk
    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    
    while (file.good()) {
        char chunkId[4];
        uint32_t chunkSize;
        
        file.read(chunkId, 4);
        file.read(reinterpret_cast<char*>(&chunkSize), 4);
        
        if (std::string(chunkId, 4) == "fmt ") {
            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            file.read(reinterpret_cast<char*>(&channels), 2);
            file.read(reinterpret_cast<char*>(&sampleRate), 4);
            file.seekg(4, std::ios::cur);  // skip byteRate
            file.seekg(2, std::ios::cur);  // skip blockAlign
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
            
            // Skip any extra format bytes
            if (chunkSize > 16) {
                file.seekg(chunkSize - 16, std::ios::cur);
            }
        }
        else if (std::string(chunkId, 4) == "data") {
            // Found data chunk
            if (audioFormat != 1 || bitsPerSample != 16) {
                throw std::runtime_error("Only 16-bit PCM is supported");
            }
            
            audio.sampleRate = sampleRate;
            audio.channels = channels;
            
            size_t numSamples = chunkSize / 2;  // 16-bit = 2 bytes per sample
            audio.samples.resize(numSamples);
            file.read(reinterpret_cast<char*>(audio.samples.data()), chunkSize);
            
            file.close();
            return true;
        }
        else {
            // Skip unknown chunk
            file.seekg(chunkSize, std::ios::cur);
        }
    }
    
    throw std::runtime_error("No data chunk found in WAV file");
}

// Write WAV file
inline bool writeWav(const std::string& filename, const AudioFile& audio) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create file: " + filename);
    }
    
    uint32_t dataSize = audio.samples.size() * 2;  // 2 bytes per sample (16-bit)
    uint32_t fileSize = 36 + dataSize;
    uint16_t audioFormat = 1;  // PCM
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = audio.sampleRate * audio.channels * 2;
    uint16_t blockAlign = audio.channels * 2;
    uint32_t fmtSize = 16;
    
    // Write RIFF header
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file.write("WAVE", 4);
    
    // Write fmt chunk
    file.write("fmt ", 4);
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&audio.channels), 2);
    file.write(reinterpret_cast<const char*>(&audio.sampleRate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    
    // Write data chunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
    file.write(reinterpret_cast<const char*>(audio.samples.data()), dataSize);
    
    file.close();
    return true;
}

// ============================================================================
// MP3 Support (Optional - requires minimp3 and LAME)
// ============================================================================
#define MINIMP3_IMPLEMENTATION
#include "External/minimp3.h"
#include "External/minimp3_ex.h"
#include <lame/lame.h>

inline bool readMp3(const std::string& filename, AudioFile& audio) {
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    
    if (mp3dec_load(&mp3d, filename.c_str(), &info, NULL, NULL)) {
        throw std::runtime_error("Cannot read MP3 file");
    }
    
    audio.sampleRate = info.hz;
    audio.channels = info.channels;
    audio.samples.assign(info.buffer, info.buffer + info.samples);
    
    free(info.buffer);
    return true;
}

inline bool writeMp3(const std::string& filename, const AudioFile& audio, int bitrate = 128) {
    lame_t lame = lame_init();
    if (!lame) return false;
    
    lame_set_num_channels(lame, audio.channels);
    lame_set_in_samplerate(lame, audio.sampleRate);
    lame_set_brate(lame, bitrate);
    lame_set_quality(lame, 2);  // 2 = high quality
    
    if (lame_init_params(lame) < 0) {
        lame_close(lame);
        return false;
    }
    
    FILE* outfile = fopen(filename.c_str(), "wb");
    if (!outfile) {
        lame_close(lame);
        return false;
    }
    
    std::vector<uint8_t> mp3Buf(audio.samples.size() * 5 / 4 + 7200);
    int numFrames = audio.samples.size() / audio.channels;
    
    int encoded = 0;
    if (audio.channels == 2) {
        // Separate stereo channels
        std::vector<int16_t> left(numFrames), right(numFrames);
        for (int i = 0; i < numFrames; i++) {
            left[i] = audio.samples[i * 2];
            right[i] = audio.samples[i * 2 + 1];
        }
        encoded = lame_encode_buffer(lame, left.data(), right.data(), 
                                     numFrames, mp3Buf.data(), mp3Buf.size());
    } else {
        encoded = lame_encode_buffer(lame, audio.samples.data(), audio.samples.data(),
                                     numFrames, mp3Buf.data(), mp3Buf.size());
    }
    
    if (encoded > 0) fwrite(mp3Buf.data(), 1, encoded, outfile);
    
    int flush = lame_encode_flush(lame, mp3Buf.data(), mp3Buf.size());
    if (flush > 0) fwrite(mp3Buf.data(), 1, flush, outfile);
    
    fclose(outfile);
    lame_close(lame);
    return true;
}
#endif

// ============================================================================
// Auto-detect format and load
// ============================================================================
inline std::string getExtension(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos) return "";
    std::string ext = filename.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

inline bool load(const std::string& filename, AudioFile& audio) {
    std::string ext = getExtension(filename);
    
    if (ext == "wav") {
        return readWav(filename, audio);
    }
    else if (ext == "mp3") {
        return readMp3(filename, audio);
    }
    else {
        throw std::runtime_error("Unsupported format: " + ext);
    }
}

inline bool save(const std::string& filename, const AudioFile& audio) {
    std::string ext = getExtension(filename);
    
    if (ext == "wav") {
        return writeWav(filename, audio);
    }
    else if (ext == "mp3") {
        return writeMp3(filename, audio);
    }
    else {
        throw std::runtime_error("Unsupported format: " + ext);
    }
}

} // namespace AudioIO

// ============================================================================
// Common Audio Processing Utilities
// ============================================================================
namespace AudioUtils {

// Convert stereo to mono
inline AudioFile stereoToMono(const AudioFile& stereo) {
    if (stereo.channels != 2) return stereo;
    
    AudioFile mono;
    mono.sampleRate = stereo.sampleRate;
    mono.channels = 1;
    
    size_t numFrames = stereo.getNumFrames();
    mono.samples.resize(numFrames);
    
    for (size_t i = 0; i < numFrames; i++) {
        mono.samples[i] = (stereo.samples[i * 2] + stereo.samples[i * 2 + 1]) / 2;
    }
    
    return mono;
}

// Convert mono to stereo
inline AudioFile monoToStereo(const AudioFile& mono) {
    if (mono.channels != 1) return mono;
    
    AudioFile stereo;
    stereo.sampleRate = mono.sampleRate;
    stereo.channels = 2;
    
    stereo.samples.resize(mono.samples.size() * 2);
    
    for (size_t i = 0; i < mono.samples.size(); i++) {
        stereo.samples[i * 2] = mono.samples[i];
        stereo.samples[i * 2 + 1] = mono.samples[i];
    }
    
    return stereo;
}

// Apply gain (volume adjustment)
inline void applyGain(AudioFile& audio, double gain) {
    for (auto& sample : audio.samples) {
        int32_t s = static_cast<int32_t>(sample * gain);
        sample = std::clamp(s, -32768, 32767);
    }
}

// Normalize audio to maximum volume without clipping
inline void normalize(AudioFile& audio) {
    int16_t maxSample = 0;
    for (auto sample : audio.samples) {
        maxSample = std::max(maxSample, static_cast<int16_t>(std::abs(sample)));
    }
    
    if (maxSample > 0) {
        double gain = 32767.0 / maxSample;
        applyGain(audio, gain);
    }
}

// Mix two audio files (must have same sample rate and channels)
inline AudioFile mix(const AudioFile& a, const AudioFile& b, double gainA = 0.5, double gainB = 0.5) {
    if (a.sampleRate != b.sampleRate || a.channels != b.channels) {
        throw std::runtime_error("Audio files must have same format for mixing");
    }
    
    AudioFile mixed = a;
    size_t minSize = std::min(a.samples.size(), b.samples.size());
    
    for (size_t i = 0; i < minSize; i++) {
        int32_t sum = static_cast<int32_t>(a.samples[i] * gainA + b.samples[i] * gainB);
        mixed.samples[i] = std::clamp(sum, -32768, 32767);
    }
    
    return mixed;
}

// Generate sine wave
inline AudioFile generateSine(double frequency, double duration, uint32_t sampleRate = 44100, uint16_t channels = 2) {
    AudioFile audio;
    audio.sampleRate = sampleRate;
    audio.channels = channels;
    
    size_t numFrames = duration * sampleRate;
    audio.samples.resize(numFrames * channels);
    
    for (size_t i = 0; i < numFrames; i++) {
        double t = static_cast<double>(i) / sampleRate;
        int16_t sample = 16000 * std::sin(2.0 * M_PI * frequency * t);
        
        for (uint16_t ch = 0; ch < channels; ch++) {
            audio.samples[i * channels + ch] = sample;
        }
    }
    
    return audio;
}

} // namespace AudioUtils