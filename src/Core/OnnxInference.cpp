#include "Core/OnnxInference.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>

DeepFilterNet::DeepFilterNet(const std::string& model_path) 
    : env_(ORT_LOGGING_LEVEL_WARNING, "DenoiserInference"),
      session_options_(),
      session_(nullptr),
      memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      allocator(),
      state_(STATE_SIZE, 0.0f),
      atten_lim_db_(0.0f) {
    
    //session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    //session_options_.SetIntraOpNumThreads(1);
    //session_options_.SetInterOpNumThreads(1);
    //session_options_.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    
    session_ = Ort::Session(env_, model_path.c_str(), session_options_);
}

DeepFilterNet::~DeepFilterNet() {    }


void DeepFilterNet::reset() {
    std::fill(state_.begin(), state_.end(), 0.0f);
    atten_lim_db_ = 0.0f;
}

std::vector<float> DeepFilterNet::denoise_frame(const std::vector<float>& frame) {
    const char* input_names[] = {"input_frame", "states", "atten_lim_db"};
    const char* output_names[] = {"enhanced_audio_frame", "new_states", "lsnr"};
    
    // Verify frame size
    if (frame.size() != HOP_SIZE) {
        throw std::runtime_error("Frame size mismatch: expected " + std::to_string(HOP_SIZE) + 
                                 " got " + std::to_string(frame.size()));
    }
    
    // Create input data
    std::vector<float> frame_data(frame);
    std::vector<float> state_data(state_);
    float atten_data = atten_lim_db_;
    
    // Create tensor shapes
    std::vector<int64_t> frame_shape = {HOP_SIZE};
    std::vector<int64_t> state_shape = {STATE_SIZE};
    std::vector<int64_t> atten_shape = {1};  // Empty for scalar
    
    // Create input tensors - use persistent data pointers
    std::vector<Ort::Value> input_tensors;
    
    input_tensors.push_back(Ort::Value::CreateTensor<float>(
        memory_info_, 
        frame_data.data(), 
        frame_data.size(), 
        frame_shape.data(), 
        frame_shape.size()
    ));
    
    input_tensors.push_back(Ort::Value::CreateTensor<float>(
        memory_info_, 
        state_data.data(), 
        state_data.size(), 
        state_shape.data(), 
        state_shape.size()
    ));
    
    input_tensors.push_back(Ort::Value::CreateTensor<float>(
        memory_info_, 
        &atten_data, 
        1, 
        atten_shape.data(), 
        atten_shape.size()
    ));
    
    // Run inference
    std::vector<Ort::Value> output_tensors = session_.Run(
        Ort::RunOptions{nullptr}, 
        input_names, 
        input_tensors.data(), 
        input_tensors.size(), 
        output_names, 
        3
    );
    
    // Extract and copy state immediately
    float* new_state_ptr = output_tensors[1].GetTensorMutableData<float>();
    auto state_info = output_tensors[1].GetTensorTypeAndShapeInfo();
    size_t state_size = state_info.GetElementCount();
    
    if (state_size != STATE_SIZE) {
        throw std::runtime_error("State size mismatch from model output");
    }
    
    // Update internal state
    std::copy(new_state_ptr, new_state_ptr + state_size, state_.begin());
    
    
    float* output_ptr = output_tensors[0].GetTensorMutableData<float>();
    auto output_info = output_tensors[0].GetTensorTypeAndShapeInfo();
    size_t output_size = output_info.GetElementCount();
  
    std::vector<float> result(output_ptr, output_ptr + output_size);
    
    return result;
}

std::vector<float> DeepFilterNet::denoise(const std::vector<float>& input_audio) {

    //Calculate padding 
    size_t orig_len = input_audio.size();
    size_t hop_padding = (HOP_SIZE - orig_len % HOP_SIZE) % HOP_SIZE;
    
    // Update orig_len to include hop padding
    orig_len += hop_padding;
    
    //Pad the input audio - pad on the RIGHT with (fft_size + hop_padding) zeros
    size_t total_padding = FFT_SIZE + hop_padding;
    std::vector<float> padded_audio;
    padded_audio.reserve(input_audio.size() + total_padding);
    padded_audio.insert(padded_audio.end(), input_audio.begin(), input_audio.end());
    padded_audio.insert(padded_audio.end(), total_padding, 0.0f);
    
    //Split into frames and process
    size_t num_frames = padded_audio.size() / HOP_SIZE;
    std::vector<float> enhanced_audio;
    enhanced_audio.reserve(num_frames * HOP_SIZE);
    
    for (size_t i = 0; i < num_frames; ++i) {
        size_t start = i * HOP_SIZE;
        
        // Create frame view without copying
        std::vector<float> frame(padded_audio.begin() + start, 
                                 padded_audio.begin() + start + HOP_SIZE);
        
        std::vector<float> output_frame = denoise_frame(frame);
        enhanced_audio.insert(enhanced_audio.end(), output_frame.begin(), output_frame.end());
    }
    
    //Remove padding using the delay formula
    size_t d = FFT_SIZE - HOP_SIZE;
    
    if (enhanced_audio.size() < d + orig_len) {
        throw std::runtime_error("Enhanced audio size mismatch");
    }
    
    std::vector<float> result(enhanced_audio.begin() + d, 
                              enhanced_audio.begin() + d + orig_len);
    
    return result;
}

// verify model input and output details
void DeepFilterNet::verify_model_io() const {
    std::cout << "Model Input/Output Information:" << std::endl;
    
    // Get input info
    size_t num_inputs = session_.GetInputCount();
    std::cout << "Number of inputs: " << num_inputs << std::endl;
    for (size_t i = 0; i < num_inputs; ++i) {
        auto name = session_.GetInputNameAllocated(i, Ort::AllocatorWithDefaultOptions());
        auto type_info = session_.GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();
        
        std::cout << "  Input " << i << ": " << name.get() << " - Shape: [";
        for (size_t j = 0; j < shape.size(); ++j) {
            std::cout << shape[j];
            if (j < shape.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }
    
    // Get output info
    size_t num_outputs = session_.GetOutputCount();
    std::cout << "Number of outputs: " << num_outputs << std::endl;
    for (size_t i = 0; i < num_outputs; ++i) {
        auto name = session_.GetOutputNameAllocated(i, Ort::AllocatorWithDefaultOptions());
        auto type_info = session_.GetOutputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();
        
        std::cout << "  Output " << i << ": " << name.get() << " - Shape: [";
        for (size_t j = 0; j < shape.size(); ++j) {
            std::cout << shape[j];
            if (j < shape.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }
}