#include "Utils/MicReader.h"
#include <iostream>

int main() {
    MicrophoneReader mic;
    
    // List available capture devices
    auto devices = mic.listDevices();
    
    if (devices.empty()) {
        std::cerr << "No microphone devices found!" << std::endl;
        return 1;
    }
    
    // Let user select capture device
    int choice;
    std::cout << "Select microphone device (0-" << devices.size() - 1 << "): ";
    std::cin >> choice;
    
    if (choice < 0 || choice >= static_cast<int>(devices.size())) {
        std::cerr << "Invalid selection!" << std::endl;
        return 1;
    }
    
    mic.selectDevice(devices[choice]);

    // Ask about monitoring (playback)
    char monitor_choice;
    std::cout << "Enable real-time monitoring (hear the mic)? (y/n): ";
    std::cin >> monitor_choice;
    if (monitor_choice == 'y' || monitor_choice == 'Y') {
        // List playback devices
        auto pb_devices = mic.listPlaybackDevices();
        int pb_choice;
        std::cout << "Select playback device (0-" << pb_devices.size() - 1 << "): ";
        std::cin >> pb_choice;
        if (pb_choice < 0 || pb_choice >= static_cast<int>(pb_devices.size())) {
            std::cerr << "Invalid playback selection, using default." << std::endl;
        } else {
            mic.selectPlaybackDevice(pb_devices[pb_choice]);
        }
        mic.setMonitorEnabled(true);
    } else {
        mic.setMonitorEnabled(false);
    }
    
    if (!mic.initialize()) {
        std::cerr << "Failed to initialize microphone!" << std::endl;
        return 1;
    }
    
    mic.processAudio();
    
    return 0;
}