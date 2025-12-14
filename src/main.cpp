#include "Utils/MicReader.h"
#include "Utils/AudReader.h"    
#include "Core/OnnxInference.h"
#include "Core/RealtimeDenoiser.h"
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
        denoiser.SetNoiseSuppressionStrength(0.0f);
        
        AudioFile audio;
        AudioIO::load(in_path, audio);
        
        cout << "Loaded audio:\n";
        cout << "  Samples: " << audio.samples.size() << "\n";
        cout << "  Sample rate: " << audio.sampleRate << " Hz\n";
        cout << "  Channels: " << audio.channels << "\n";
        cout << "  Duration: " << audio.getDuration() << " seconds\n";

        vector<float> float_samples;
        float_samples.reserve(audio.samples.size());
        
        for (const auto& sample : audio.samples) {
            float_samples.push_back(static_cast<float>(sample) / 32768.0f);
        }
        
        float input_peak = 0.0f;
        for (float s : float_samples) {
            input_peak = std::max(input_peak, std::abs(s));
        }
        cout << "Input peak level: " << input_peak << "\n";

        cout << "\nProcessing through DeepFilterNet...\n";
        vector<float> denoised = denoiser.ApplyNoiseSuppression(float_samples);
        
        float output_peak = 0.0f;
        for (float s : denoised) {
            output_peak = std::max(output_peak, std::abs(s));
        }
        cout << "Output peak level: " << output_peak << "\n";

        audio.samples.clear();
        audio.samples.reserve(denoised.size());
        
        for (const auto& sample : denoised) {
            float scaled = sample * 32767.0f;
            int32_t clamped = std::clamp(
                static_cast<int32_t>(scaled),
                static_cast<int32_t>(-32768),
                static_cast<int32_t>(32767)
            );
            audio.samples.push_back(static_cast<int16_t>(clamped));
        }

        AudioIO::save(out_path, audio);
        cout << "\n✓ Saved: " << out_path << "\n";
        cout << "  Output samples: " << audio.samples.size() << "\n";
        cout << "  Duration: " << audio.getDuration() << " seconds\n";
        
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

static int run_realtime_mode() {
    try {
        RealtimeDenoiser denoiser;
        
        // Load model
        const string model = "../assets/models/DeepFilterNetV3.onnx";
        if (!denoiser.loadModel(model)) {
            return 1;
        }
        
        // Set noise suppression strength
        cout << "\nNoise Suppression Strength:\n";
        cout << "  0   = Minimal (best quality)\n";
        cout << " -50  = Gentle\n";
        cout << " -75  = Balanced\n";
        cout << "-100  = Aggressive (may affect voice)\n";
        cout << "Enter value (-100 to 0): ";
        
        float strength = 0.0f;
        std::cin >> strength;
        std::cin.ignore();
        
        denoiser.setNoiseSuppressionStrength(strength);
        
        // Select microphone
        auto mics = denoiser.listMicrophones();
        if (mics.empty()) {
            cerr << "No microphones found!\n";
            return 1;
        }
        
        cout << "\nAvailable microphones:\n";
        for (size_t i = 0; i < mics.size(); ++i) {
            cout << "  [" << i << "] " << mics[i] << "\n";
        }
        
        int mic_choice = 0;
        cout << "Select microphone (0-" << mics.size() - 1 << "): ";
        std::cin >> mic_choice;
        std::cin.ignore();
        
        denoiser.selectMicrophone(mic_choice);
        
        // Enable monitoring
        char monitor = 'n';
        cout << "Enable real-time monitoring (hear output)? (y/n): ";
        std::cin >> monitor;
        std::cin.ignore();
        
        if (monitor == 'y' || monitor == 'Y') {
            auto speakers = denoiser.listSpeakers();
            if (!speakers.empty()) {
                cout << "\nAvailable speakers:\n";
                for (size_t i = 0; i < speakers.size(); ++i) {
                    cout << "  [" << i << "] " << speakers[i] << "\n";
                }
                
                int speaker_choice = 0;
                cout << "Select speaker (0-" << speakers.size() - 1 << "): ";
                std::cin >> speaker_choice;
                std::cin.ignore();
                
                denoiser.selectSpeaker(speaker_choice);
            }
            denoiser.enableMonitoring(true);
        }
        
        // Initialize and start
        if (!denoiser.initialize()) {
            return 1;
        }
        
        denoiser.start();
        
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

static int run_mic_test() {
    MicrophoneReader mic;

    auto devices = mic.listDevices();
    if (devices.empty()) {
        cerr << "No microphone devices found!\n";
        return 1;
    }

    int choice = 0;
    cout << "Select microphone device (0-" << devices.size() - 1 << "): ";
    if (!(std::cin >> choice) || choice < 0 || choice >= static_cast<int>(devices.size())) {
        cerr << "Invalid selection\n";
        return 1;
    }
    mic.selectDevice(devices[choice]);

    char monitor_choice = 'n';
    cout << "Enable real-time monitoring? (y/n): ";
    std::cin >> monitor_choice;
    
    if (monitor_choice == 'y' || monitor_choice == 'Y') {
        auto pb_devices = mic.listPlaybackDevices();
        int pb_choice = 0;
        cout << "Select playback device (0-" << pb_devices.size() - 1 << "): ";
        if ((std::cin >> pb_choice) && pb_choice >= 0 && pb_choice < static_cast<int>(pb_devices.size())) {
            mic.selectPlaybackDevice(pb_devices[pb_choice]);
        }
        mic.setMonitorEnabled(true);
    }

    if (!mic.initialize()) {
        cerr << "Failed to initialize microphone!\n";
        return 1;
    }

    mic.processAudio();
    mic.cleanup();
    return 0;
}

static int run_audio_reader_test(const string& in_path, const string& out_path) {
    try {
        AudioFile audio;
        AudioIO::load(in_path, audio);
        AudioUtils::normalize(audio);
        AudioIO::save(out_path, audio);
        return 0;
    } catch (const exception& e) {
        cerr << "Audio Reader error: " << e.what() << "\n";
        return 1;
    }
}

int main(int argc, char* argv[]) {
    if (argc == 3) {
        // File mode: ./NeuralMic input.wav output.wav
        return run_file_mode(argv[1], argv[2]);
    } else if (argc == 2 && string(argv[1]) == "--realtime") 
    {
        // Real-time mode: ./NeuralMic --realtime
        return run_realtime_mode();
    }
      else if (argc == 2 && string(argv[1]) == "--test-mic") {
        // Microphone test mode: ./NeuralMic --test-mic
        return run_mic_test();
    }
    
    // Default: Real-time mode
    return run_realtime_mode();
}