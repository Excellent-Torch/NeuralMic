#include "Core/OnnxInference.h"
#include <fstream>
#include <stdexcept>

DeepFilterNet::DeepFilterNet(const std::string& model_path) 
    : env(ORT_LOGGING_LEVEL_WARNING, "DeepFilterNet"),
      session(env, model_path.c_str(), Ort::SessionOptions()),
      memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
    
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(4);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

std::vector<float> DeepFilterNet::denoise(const std::vector<float>& audio) {
    
    // DeepFilterNet expects input shape: [batch, channels, samples]
    // Process in chunks of ~10 seconds (48000 samples at 48kHz)
    const int chunk_size = 48000;
    const int hop_size = 24000; // 50% overlap
    std::vector<float> output(audio.size(), 0.0f);
    std::vector<float> weights(audio.size(), 0.0f);
    
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    
    for (int start = 0; start < audio.size(); start += hop_size) {
        int end = std::min(start + chunk_size, (int)audio.size());
        int size = end - start;
        
        std::vector<float> chunk(chunk_size, 0.0f);
        std::copy(audio.begin() + start, audio.begin() + end, chunk.begin());
        
        std::vector<int64_t> input_shape = {1, 1, chunk_size};
        auto input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, chunk.data(), chunk.size(), input_shape.data(), 3);
        
        const char* input_names[] = {"input"};
        const char* output_names[] = {"output"};
        
        auto output_tensors = session.Run(Ort::RunOptions{nullptr}, 
            input_names, &input_tensor, 1, output_names, 1);
        
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        int output_size = output_shape[2];
        
        // Apply Hann window for smooth overlap-add
        for (int i = 0; i < std::min(size, output_size); i++) {
            float window = 0.5f * (1.0f - cos(2.0f * M_PI * i / size));
            output[start + i] += output_data[i] * window;
            weights[start + i] += window;
        }
    }
    
    // Normalize by window weights
    for (int i = 0; i < output.size(); i++) {
        if (weights[i] > 0.0f) {
            output[i] /= weights[i];
        }
    }
    
    return output;
}

AudioData load_wav(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open: " + path);
    
    file.seekg(24); 
    uint32_t rate; file.read((char*)&rate, 4);
    
    file.seekg(40); 
    uint32_t size; file.read((char*)&size, 4);
    
    std::vector<int16_t> pcm(size / 2);
    file.read((char*)pcm.data(), size);
    
    std::vector<float> samples(pcm.size());
    for (size_t i = 0; i < pcm.size(); ++i)
        samples[i] = pcm[i] / 32768.0f;
    
    return {rate, samples};
}

void save_wav(const std::string& path, const AudioData& audio) {
    std::ofstream file(path, std::ios::binary);
    
    uint32_t data_sz = audio.samples.size() * 2;
    uint32_t file_sz = data_sz + 36;
    uint16_t fmt = 1, ch = 1, bits = 16;
    uint32_t rate = audio.sample_rate, byte_rate = rate * 2;
    uint16_t align = 2;
    
    file.write("RIFF", 4); file.write((char*)&file_sz, 4);
    file.write("WAVEfmt ", 8); 
    uint32_t t = 16; file.write((char*)&t, 4);
    file.write((char*)&fmt, 2); file.write((char*)&ch, 2);
    file.write((char*)&rate, 4); file.write((char*)&byte_rate, 4);
    file.write((char*)&align, 2); file.write((char*)&bits, 2);
    file.write("data", 4); file.write((char*)&data_sz, 4);
    
    for (float s : audio.samples) {
        int16_t pcm = s * 32767.0f;
        file.write((char*)&pcm, 2);
    }
}