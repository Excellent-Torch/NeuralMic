# NeuralMic

Real-time AI-based noise suppression for Linux using DeepFilterNet.

## Overview

NeuralMic is a C++ application that leverages the [DeepFilterNet](https://github.com/Rikorose/DeepFilterNet) neural network model to perform real-time audio noise suppression. It can process live microphone input or denoise audio files, making it ideal for improving voice clarity in noisy environments.

## Features

- **Real-time Noise Suppression** – Process microphone audio on-the-fly with minimal latency
- **File Processing Mode** – Denoise WAV audio files
- **Adjustable Suppression Strength** – Fine-tune noise reduction from gentle to aggressive
- **Device Selection** – Choose input microphone and output speaker devices
- **Live Monitoring** – Hear the processed audio in real-time
- **Cross-platform Audio** – Built on libsoundio for robust audio handling
- **ONNX Runtime** – Efficient neural network inference

## Requirements

### System Dependencies

- Linux (tested on Ubuntu/Debian-based systems)
- CMake 3.28+
- C++23 compatible compiler (g++-13 or later)
- libsoundio

### Installing Dependencies

**Ubuntu/Debian:**

```bash
# Build essentials
sudo apt-get install build-essential cmake g++-13

# Audio library
sudo apt-get install libsoundio-dev

# Optional: MP3 support
sudo apt-get install libmp3lame-dev
```

**Building libsoundio from source (if not available via package manager):**

```bash
git clone https://github.com/andrewrk/libsoundio.git
cd libsoundio
mkdir build && cd build
cmake ..
make
sudo make install
```

## Building

```bash
# Clone the repository
git clone https://github.com/yourusername/NeuralMic.git
cd NeuralMic

# Create build directory
mkdir build && cd build

# Configure (ONNX Runtime will be downloaded automatically)
cmake ..

# Build
make -j$(nproc)
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `FETCH_ONNXRUNTIME` | `ON` | Automatically download ONNX Runtime |
| `ENABLE_MP3` | `OFF` | Enable MP3 file support (requires LAME) |

Example with options:

```bash
cmake .. -DFETCH_ONNXRUNTIME=ON -DENABLE_MP3=ON
```

## Usage

### Real-time Mode (Default)

Run the application without arguments or with `--realtime` flag:

```bash
./NeuralMic
# or
./NeuralMic --realtime
```

You will be prompted to:
1. Set noise suppression strength (0 to -100 dB)
2. Select your microphone device
3. Optionally enable monitoring and select a speaker

**Suppression Strength Guide:**
| Value | Effect |
|-------|--------|
| `0` | Minimal suppression (best audio quality) |
| `-50` | Gentle noise reduction |
| `-75` | Balanced (recommended starting point) |
| `-100` | Aggressive (may affect voice quality) |

### File Processing Mode

Process a WAV file:

```bash
./NeuralMic input.wav output.wav
```

### Microphone Test Mode

Test your microphone setup without noise suppression:

```bash
./NeuralMic --test-mic
```

## Project Structure

```
NeuralMic/
├── assets/
│   └── models/
│       └── DeepFilterNetV3.onnx    # Neural network model
├── include/
│   ├── Core/
│   │   ├── OnnxInference.h         # DeepFilterNet wrapper
│   │   └── RealtimeDenoiser.h      # Real-time processing controller
│   ├── External/
│   │   └── minimp3*.h              # MP3 decoding (header-only)
│   └── Utils/
│       ├── AudReader.h             # Audio file I/O
│       └── MicReader.h             # Microphone capture
├── src/
│   ├── main.cpp                    # Application entry point
│   ├── Core/
│   │   ├── OnnxInference.cpp       # ONNX model inference
│   │   └── RealtimeDenoiser.cpp    # Real-time pipeline
│   └── Utils/
│       └── MicReader.cpp           # Audio device handling
├── CMakeLists.txt
└── README.md
```

## Technical Details

- **Sample Rate:** 48 kHz
- **Frame Size:** 480 samples (10ms hop size)
- **FFT Size:** 960 samples
- **Model:** DeepFilterNet V3 (ONNX format)
- **Inference:** ONNX Runtime 1.23.2

## Troubleshooting

### No microphones found
Ensure your audio devices are properly configured and accessible:
```bash
# List audio devices
aplay -l
arecord -l
```

### Model loading fails
Make sure the model file exists at `assets/models/DeepFilterNetV3.onnx` relative to your working directory. When running from the build folder, use:
```bash
cd build
./NeuralMic  # Model path is ../assets/models/DeepFilterNetV3.onnx
```

### High latency or audio glitches
- Try reducing other system audio processing
- Ensure you're running on a system with adequate CPU performance
- Consider adjusting the suppression strength

## License

See [LICENSE](LICENSE) file for details.

## Acknowledgments

- [DeepFilterNet](https://github.com/Rikorose/DeepFilterNet) – The neural network model for noise suppression
- [ONNX Runtime](https://github.com/microsoft/onnxruntime) – High-performance inference engine
- [libsoundio](https://github.com/andrewrk/libsoundio) – Cross-platform audio I/O library
- [minimp3](https://github.com/lieff/minimp3) – Minimalistic MP3 decoder
