#include "Utils/MicReader.h"
#include <iostream>

int main() {
    MicrophoneReader mic;
    
    // List available devices
    auto devices = mic.listDevices();
    
    if (devices.empty()) {
        std::cerr << "No microphone devices found!" << std::endl;
        return 1;
    }
    
    // Let user select device
    int choice;
    std::cout << "Select microphone device (0-" << devices.size() - 1 << "): ";
    std::cin >> choice;
    
    if (choice < 0 || choice >= static_cast<int>(devices.size())) {
        std::cerr << "Invalid selection!" << std::endl;
        return 1;
    }
    
    // Select and initialize
    mic.selectDevice(devices[choice]);
    
    if (!mic.initialize()) {
        std::cerr << "Failed to initialize microphone!" << std::endl;
        return 1;
    }
    
    mic.processAudio();
    
    return 0;
}