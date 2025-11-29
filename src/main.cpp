#include "Utils/MicReader.h"
#include "Core/OnnxInference.h"
#include <iostream>
#include <string>
#include <filesystem>

#ifdef HAVE_ONNXRUNTIME
using ModelAvailable = std::true_type;
#else
using ModelAvailable = std::false_type;
#endif

// Under Development: File mode to denoise a WAV file using DeepFilterNet
static int run_file_mode(const std::string& in_path, const std::string& out_path) {
#ifdef HAVE_ONNXRUNTIME
    try {
        const std::string model = "/media/warehouse/Projects/NeuralMic/assets/models/enc.onnx";
        std::cout << "Loading model: " << std::filesystem::current_path() << "\n";
        DeepFilterNet denoiser(model);
        auto audio = load_wav(in_path);
        std::cout << "Processing " << audio.samples.size() << " samples...\n";
        audio.samples = denoiser.denoise(audio.samples);
        save_wav(out_path, audio);
        std::cout << "Saved: " << out_path << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Denoise error: " << e.what() << "\n";
        return 1;
    }
#else
    std::cerr << "ONNX Runtime support not available. Rebuild with ONNX enabled.\n";
    return 2;
#endif
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

int main() {
    // Uncomment to run file mode test
    //return run_file_mode("/home/torch/Music/jackhammer.wav", "/home/torch/Music/out.wav");

    // Microphone test mode
    return run_mic_test();
}