#include "Utils/MicReader.h"
#include "Utils/AudReader.h"    
#include "Core/OnnxInference.h"
#include <iostream>
#include <string>
#include <filesystem>

using std::string;
using std::cout;
using std::cerr;
using std::exception;
using std::vector;

static int run_file_mode(const string& in_path, const string& out_path) {
   try {
        const string model = "../assets/models/DeepFilterNetV3.onnx";
        DeepFilterNet denoiser(model);
        
        // Adjust noise reduction strength:
        //  0.0  = Default (Better)
        // -100.0 = Aggressive (Lower Voice Quality)
        // -75.0 = Balanced
        // -50.0  = Gentle
        denoiser.SetNoiseSuppressionStrength(0.0f);
        
        AudioFile audio;
        AudioIO::load(in_path, audio);
        
        cout << "Loaded audio:\n";
        cout << "  Samples: " << audio.samples.size() << "\n";
        cout << "  Sample rate: " << audio.sampleRate << " Hz\n";
        cout << "  Channels: " << audio.channels << "\n";
        cout << "  Duration: " << audio.getDuration() << " seconds\n";

        // Convert int16_t to normalized float [-1.0, 1.0]
        vector<float> float_samples;
        float_samples.reserve(audio.samples.size());
        
        for (const auto& sample : audio.samples) {
            // CRITICAL: Normalize to [-1.0, 1.0] range
            float_samples.push_back(static_cast<float>(sample) / 32768.0f);
        }
        
        // Check input level
        float input_peak = 0.0f;
        for (float s : float_samples) {
            input_peak = std::max(input_peak, std::abs(s));
        }
        cout << "Input peak level: " << input_peak << "\n";
        
        if (input_peak < 0.01f) {
            cerr << "Warning: Input audio is very quiet (peak < 0.01)\n";
        }

        // Process through denoiser
        cout << "\nProcessing through DeepFilterNet...\n";
        vector<float> denoised = denoiser.ApplyNoiseSuppression(float_samples);
        
        // Check output level
        float output_peak = 0.0f;
        for (float s : denoised) {
            output_peak = std::max(output_peak, std::abs(s));
        }
        cout << "Output peak level: " << output_peak << "\n";

        // Convert back to int16_t with proper scaling and clamping
        audio.samples.clear();
        audio.samples.reserve(denoised.size());
        
        for (const auto& sample : denoised) {
            // Scale back to int16_t range with clamping
            float scaled = sample * 32767.0f;
            int32_t clamped = std::clamp(
                static_cast<int32_t>(scaled),
                static_cast<int32_t>(-32768),
                static_cast<int32_t>(32767)
            );
            audio.samples.push_back(static_cast<int16_t>(clamped));
        }

        AudioIO::save(out_path, audio);
        cout << "\n Saved: " << out_path << "\n";
        cout << "  Output samples: " << audio.samples.size() << "\n";
        cout << "  Duration: " << audio.getDuration() << " seconds\n";
        
        return 0;
        
    } catch (const exception& e) {
        cerr << "✗ Denoise error: " << e.what() << "\n";
        return 1;
    }
}


// Microphone test mode
static int run_mic_test() {
    MicrophoneReader mic;

    // List capture devices
    auto devices = mic.listDevices();
    if (devices.empty()) {
        std::cerr << "No microphone devices found!\n";
        return 1;
    }

    int choice = 0;
    std::cout << "Select microphone device (0-" << devices.size() - 1 << "): ";
    if (!(std::cin >> choice) || choice < 0 || choice >= static_cast<int>(devices.size())) {
        std::cerr << "Invalid selection\n";
        return 1;
    }
    mic.selectDevice(devices[choice]);

    // Monitoring choice
    char monitor_choice = 'n';
    std::cout << "Enable real-time monitoring (hear the mic)? (y/n): ";
    std::cin >> monitor_choice;
    if (monitor_choice == 'y' || monitor_choice == 'Y') {
        auto pb_devices = mic.listPlaybackDevices();
        int pb_choice = 0;
        std::cout << "Select playback device (0-" << pb_devices.size() - 1 << "): ";
        if ((std::cin >> pb_choice) && pb_choice >= 0 && pb_choice < static_cast<int>(pb_devices.size())) {
            mic.selectPlaybackDevice(pb_devices[pb_choice]);
        } else {
            std::cout << "Using default playback device\n";
        }
        mic.setMonitorEnabled(true);
    } else {
        mic.setMonitorEnabled(false);
    }

    if (!mic.initialize()) {
        std::cerr << "Failed to initialize microphone!\n";
        return 1;
    }

    mic.processAudio(); // blocks until user interrupts
    mic.cleanup();
    return 0;
}

// Audio Reader Test Application
static int run_audio_reader_test(const std::string& in_path, const std::string& out_path) {
    try {
        AudioFile audio;
        AudioIO::load(in_path, audio);      // Auto-detects format
        AudioUtils::normalize(audio);           // Process
        AudioIO::save(out_path, audio);     // Save
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Audio Reader error: " << e.what() << "\n";
        return 1;
    }
}
    

int main() {
    // Uncomment to run file mode test
    return run_file_mode("../assets/tests/input.wav", "../assets/tests/output.wav");

    // Audio Reader test mode
    //return run_audio_reader_test("/home/torch/Music/jackhammerm.mp3", "/home/torch/Music/output.mp3");
    // Microphone test mode
    //return run_mic_test();
}