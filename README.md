<p align="center">
  <img src="assets/images/logo.png" alt="NeuralMic Logo" width="256">
</p>

<h1 align="center"></h1>

<p align="center">
  <strong>Real-time AI-based noise suppression for Linux using DeepFilterNet</strong>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPL v3"></a>
</p>

## Overview

NeuralMic is a C++ application that leverages the [DeepFilterNet](https://github.com/Rikorose/DeepFilterNet) neural network model to perform real-time audio noise suppression. It can process live microphone input or denoise audio files, making it ideal for improving voice clarity in noisy environments. 

⚠️ **Currently the GUI version is still under development.** 

## Features

- **Qt GUI Application** – Modern, responsive interface with real-time controls
- **Real-time Noise Suppression** – Process microphone audio on-the-fly with minimal latency
- **Virtual Microphone** – Output to a virtual device for use with Discord, Zoom, etc.
- **File Processing Mode** – Denoise WAV audio files (Under Development for the GUI)
- **Adjustable Suppression Strength** – Fine-tune noise reduction from 0% to 100%
- **Device Selection** – Choose input microphone and output speaker devices
- **Live Monitoring** – Toggle real-time audio monitoring during processing
- **Threaded Audio Processing** – Non-blocking UI with separate audio thread
- **Cross-platform Audio** – Built on libsoundio for robust audio handling
- **ONNX Runtime** – Efficient neural network inference

## Using with Discord, Zoom, and Other Apps

NeuralMic creates a virtual microphone called **"Monitor of NeuralMic"** that you can select as your input device in any application:

1. Start NeuralMic with "Virtual Mic" enabled (enabled by default)
2. Click "Start" to begin processing
3. In Discord/Zoom/etc., go to audio settings and select **"Monitor of NeuralMic"** as your input device
4. Your voice will now have real-time noise suppression!

## Screenshots

### Qt GUI
The GUI provides an intuitive interface for:
- Audio device selection (input/output)
- Real-time noise suppression control (0-100%)
- Live monitoring toggle
- One-click start/stop with visual feedback
- Status display with error handling

## Requirements

### System Dependencies

- Linux (tested on Ubuntu/Debian-based systems)
- CMake 3.28+
- C++23 compatible compiler (g++-13 or later)
- libsoundio
- Qt6 (or Qt5) – for GUI application

### Installing Dependencies

**Ubuntu/Debian:**

```bash
# Build essentials
sudo apt-get install build-essential cmake g++-13

# Audio library
sudo apt-get install libsoundio-dev

# PulseAudio for virtual microphone
sudo apt-get install libpulse-dev

# Qt6 for GUI (or use qt5-default for Qt5)
sudo apt-get install qt6-base-dev

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
| `BUILD_GUI` | `ON` | Build Qt GUI application |
| `FETCH_ONNXRUNTIME` | `ON` | Automatically download ONNX Runtime |
| `ENABLE_MP3` | `OFF` | Enable MP3 file support (requires LAME) |

Example with options:

```bash
cmake .. -DBUILD_GUI=ON -DFETCH_ONNXRUNTIME=ON -DENABLE_MP3=ON
```

## Usage

### Qt GUI Application (Recommended)

Launch the graphical interface:

```bash
./build/NeuralMicGUI
```

**GUI Features:**
1. **Device Selection** – Choose input microphone and output speaker from dropdowns
2. **Monitoring** – Check "Monitor output" to hear processed audio in real-time
3. **Noise Suppression** – Adjust strength slider (0% = minimal, 100% = aggressive)
4. **Start/Stop** – Click to begin/end audio processing
5. **Refresh** – Reload audio devices if needed

**Suppression Strength Guide:**
| Value | Effect |
|-------|--------|
| `0%` | Minimal suppression (best audio quality) |
| `33%` | Gentle noise reduction |
| `66%` | Balanced (recommended starting point) |
| `100%` | Aggressive (may affect voice quality) |

### Command-Line Interface

#### Real-time Mode (Default)

Run without arguments:

```bash
./build/NeuralMic
```

You will be prompted to:
1. Set noise suppression strength (0 to -100 dB)
2. Select your microphone device
3. Optionally enable monitoring and select a speaker

#### File Processing Mode

Process a WAV file:

```bash
./build/NeuralMic input.wav output.wav
```

#### Microphone Test Mode

Test your microphone setup without noise suppression:

```bash
./build/NeuralMic --test-mic
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
│   ├── main.cpp                    # CLI entry point
│   ├── gui/
│   │   ├── main.cpp                # GUI entry point
│   │   ├── MainWindow.h            # Main window interface
│   │   └── MainWindow.cpp          # GUI implementation
│   ├── Core/
│   │   ├── OnnxInference.cpp       # ONNX model inference
│   │   └── RealtimeDenoiser.cpp    # Real-time pipeline
│   └── Utils/
│       └── MicReader.cpp           # Audio device handling
├── .vscode/
│   └── c_cpp_properties.json       # IntelliSense configuration
├── CMakeLists.txt
└── README.md
```

## Technical Details

- **Sample Rate:** 48 kHz
- **Frame Size:** 480 samples (10ms hop size)
- **FFT Size:** 960 samples
- **Model:** DeepFilterNet V3 (ONNX format)
- **Inference:** ONNX Runtime 1.23.2
- **GUI Framework:** Qt6 (with Qt5 fallback)
- **Audio Backend:** libsoundio (supports ALSA, PulseAudio, JACK)

## Architecture

### GUI Application
- **MainWindow** – Qt-based user interface with device controls
- **AudioWorker** – Separate thread for non-blocking audio processing
- **Signal/Slot System** – Thread-safe communication between UI and audio thread
- **Real-time Updates** – Dynamic monitoring toggle and device refresh

### Audio Processing Pipeline
1. Microphone capture (libsoundio)
2. Frame buffering (480 samples @ 48kHz)
3. Neural network inference (DeepFilterNet via ONNX Runtime)
4. Optional monitoring output
5. Virtual device creation for system-wide noise suppression

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
./NeuralMicGUI  # Model path is ../assets/models/DeepFilterNetV3.onnx
```

### GUI doesn't start / Qt not found
Ensure Qt is installed:
```bash
sudo apt-get install qt6-base-dev
# or for Qt5
sudo apt-get install qtbase5-dev
```

If Qt is installed but not detected, try specifying the path:
```bash
cmake .. -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt6
```

### High latency or audio glitches
- Try reducing other system audio processing
- Ensure you're running on a system with adequate CPU performance
- Consider adjusting the suppression strength to a lower value
- Disable monitoring if not needed to reduce processing overhead

### Start button doesn't change to Stop
This is fixed in the latest version. If you encounter this:
1. Make sure you're running the latest build
2. Check terminal output for errors
3. Try refreshing devices before starting

## Performance Tips

- **Lower suppression strength** = less CPU usage
- **Disable monitoring** when not needed to reduce audio pipeline overhead
- **Close other audio applications** to avoid conflicts
- **Use ALSA directly** instead of PulseAudio for lower latency (advanced users)

## License

See [LICENSE](LICENSE) file for details.

## Acknowledgments

- [DeepFilterNet](https://github.com/Rikorose/DeepFilterNet) – The neural network model for noise suppression
- [ONNX Runtime](https://github.com/microsoft/onnxruntime) – High-performance inference engine
- [libsoundio](https://github.com/andrewrk/libsoundio) – Cross-platform audio I/O library
- [minimp3](https://github.com/lieff/minimp3) – Minimalistic MP3 decoder
- [Qt](https://www.qt.io/) – Cross-platform GUI framework

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Roadmap

- [ ] PipeWire virtual device support
- [ ] System tray integration
- [ ] Audio quality metrics display
- [ ] Profile presets for different use cases
- [ ] Windows and macOS support
- [ ] Plugin system for additional noise reduction models
