#include "Utils/MicReader.h"
#include "Utils/AudReader.h"    
#include "Core/OnnxInference.h"
#include <iostream>
#include <string>
#include <filesystem>



static int run_file_mode(const std::string& in_path, const std::string& out_path) {
    try {
        const std::string model = "/media/warehouse/Projects/NeuralMic/assets/models/DeepFilterNetV3.onnx";
        std::cout << "Loading model: " << std::filesystem::current_path() << "\n";
        DeepFilterNet denoiser(model);
        denoiser.verify_model_io();
        
        AudioFile audio;
        AudioIO::load(in_path, audio);
        AudioUtils::normalize(audio);
        AudioUtils::stereoToMono(audio);

        std::cout << "Processing " << audio.samples.size() << " samples...\n";

        std::vector<float> float_samples;
        float_samples.reserve(audio.samples.size());

        for (const auto& sample : audio.samples) {
            float_samples.push_back(static_cast<float>(sample));
        }

        std::vector<float> denoised = denoiser.denoise(float_samples);

        audio.samples.clear();
        audio.samples.reserve(denoised.size());
        
        for (const auto& sample : denoised) {
            audio.samples.push_back(static_cast<int16_t>(sample));
        }
        AudioUtils::monoToStereo(audio);

        AudioIO::save(out_path, audio);
        std::cout << "Saved: " << out_path << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Denoise error: " << e.what() << "\n";
        return 1;
    }

    std::cerr << "ONNX Runtime support not available. Rebuild with ONNX enabled.\n";
    return 2;
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
    return run_file_mode("/home/torch/Music/jackhammerw.wav", "/home/torch/Music/out.wav");

    // Audio Reader test mode
    //return run_audio_reader_test("/home/torch/Music/jackhammerm.mp3", "/home/torch/Music/output.mp3");
    // Microphone test mode
    //return run_mic_test();
}