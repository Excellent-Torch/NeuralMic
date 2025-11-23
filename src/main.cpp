#include "Utils/MicReader.h"
#include <iostream>

int main() {
    MicrophoneReader mic;
    
    if (!mic.initialize()) {
        std::cerr << "Failed to initialize microphone!" << std::endl;
        return 1;
    }
    
    mic.processAudio();
    
    return 0;
}