#include "Core/OnnxInference.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <string>
#include <filesystem>

using std::vector; 
using std::string;
using std::fill;
using std::cout;
using std::array;
using std::runtime_error;
using std::move;
using std::copy;
using std::cerr;

DeepFilterNet::DeepFilterNet(const std::string& model_path) 
    : env_(ORT_LOGGING_LEVEL_WARNING, "DenoiserInference"),
      session_options_(),
      session_(nullptr),
      memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      allocator(),
      state_(STATE_SIZE, 0.0f),
      atten_lim_db_(0.0f) {

    session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    session_options_.SetIntraOpNumThreads(1);
    session_options_.SetInterOpNumThreads(1);
    session_options_.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    
    session_ = Ort::Session(env_, model_path.c_str(), session_options_);
    PrintModelSummary();
}

DeepFilterNet::~DeepFilterNet() {  }

void DeepFilterNet::reset() 
{
    std::fill(state_.begin(), state_.end(), 0.0f);
}

void DeepFilterNet::SetNoiseSuppressionStrength(float db) 
{
    // db range: -100.0 (very aggressive) to -50.0 (gentle)
    atten_lim_db_ = std::clamp(db, -100.0f, 0.0f);
    cout << "Attenuation set to: " << atten_lim_db_ << " dB\n";
}

vector<float> DeepFilterNet::ApplyNoiseSuppression(const vector<float>& audio) 
{
    if (audio.empty()) 
    {
        throw std::runtime_error("Input audio is empty");
    }

    // Prepare padded audio
    auto padded = GetPaddedAudio(audio);
    int orig_len = audio.size() + ((HOP_SIZE - (audio.size() % HOP_SIZE)) % HOP_SIZE);
    
    cout << "Processing " << (padded.size() / HOP_SIZE) << " frames...\n";

    // Process each frame
    vector<float> enhanced;
    enhanced.reserve(padded.size());
    
    for (size_t i = 0; i + HOP_SIZE <= padded.size(); i += HOP_SIZE) 
    {
        vector<float> frame(padded.begin() + i, padded.begin() + i + HOP_SIZE);
        auto enhanced_frame = GetEnhancedFrame(frame);
        enhanced.insert(enhanced.end(), enhanced_frame.begin(), enhanced_frame.end());
    }

    // Trim padding and return
    return GetTrimmedOutput(enhanced, orig_len);
}

vector<float> DeepFilterNet::ProcessRealtimeFrame(const vector<float>& frame) 
{
    if (frame.size() != HOP_SIZE) 
    {
        throw std::runtime_error("Frame size must be exactly " + std::to_string(HOP_SIZE) + " samples");
    }
    
    // Process frame directly without padding (state persists between calls)
    return GetEnhancedFrame(frame);
}

vector<float> DeepFilterNet::GetPaddedAudio(const vector<float>& audio) 
{
    int hop_padding = (HOP_SIZE - (audio.size() % HOP_SIZE)) % HOP_SIZE;
    int total_padding = FFT_SIZE + hop_padding;
    
    vector<float> padded = audio;
    padded.resize(audio.size() + total_padding, 0.0f);
    return padded;
}

vector<float> DeepFilterNet::GetEnhancedFrame(const vector<float>& frame) 
{
    // Create input tensors
    int64_t frame_shape[] = {HOP_SIZE};
    int64_t state_shape[] = {STATE_SIZE};
    int64_t atten_shape[] = {1};
    
    float atten = atten_lim_db_;

    Ort::Value input_frame = Ort::Value::CreateTensor<float>(
        memory_info_, const_cast<float*>(frame.data()), frame.size(), frame_shape, 1);
    
    Ort::Value input_state = Ort::Value::CreateTensor<float>(
        memory_info_, state_.data(), state_.size(), state_shape, 1);
    
    Ort::Value input_atten = Ort::Value::CreateTensor<float>(
        memory_info_, &atten, 1, atten_shape, 1);

    // Run inference
    const char* input_names[] = {"input_frame", "states", "atten_lim_db"};
    const char* output_names[] = {"enhanced_audio_frame", "new_states", "lsnr"};
    
    Ort::Value inputs[] = {
        std::move(input_frame),
        std::move(input_state),
        std::move(input_atten)
    };

    auto outputs = session_.Run(
        Ort::RunOptions{nullptr},
        input_names, inputs, 3,
        output_names, 3
    );

    // Extract enhanced frame
    float* enhanced_data = outputs[0].GetTensorMutableData<float>();
    size_t enhanced_count = outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
    
    // Update state
    float* new_state = outputs[1].GetTensorMutableData<float>();
    size_t state_count = outputs[1].GetTensorTypeAndShapeInfo().GetElementCount();
    std::copy(new_state, new_state + std::min(state_count, (size_t)STATE_SIZE), state_.begin());

    return vector<float>(enhanced_data, enhanced_data + enhanced_count);
}

vector<float> DeepFilterNet::GetTrimmedOutput(const vector<float>& enhanced, int orig_len) 
{
    int d = FFT_SIZE - HOP_SIZE;
    int start = d;
    int end = std::min(orig_len + d, (int)enhanced.size());
    
    if (start < 0 || start >= end || end > (int)enhanced.size()) 
    {
        cerr << "Warning: Invalid trim range\n";
        return enhanced;
    }
    
    return vector<float>(enhanced.begin() + start, enhanced.begin() + end);
}

// verify model input and output details
void DeepFilterNet::PrintModelSummary() const 
{
   cout << "\n=== Model I/O Verification ===\n";
    
    size_t num_inputs = session_.GetInputCount();
    cout << "Inputs: " << num_inputs << '\n';
    for (size_t i = 0; i < num_inputs; ++i) 
    {
        auto name = session_.GetInputNameAllocated(i, Ort::AllocatorWithDefaultOptions());
        auto type_info = session_.GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();
        
        cout << "  [" << i << "] " << name.get() << " - Shape: [";
        for (size_t j = 0; j < shape.size(); ++j) {
            cout << shape[j];
            if (j < shape.size() - 1) cout << ", ";
        }
        cout << "]\n";
    }
    
    size_t num_outputs = session_.GetOutputCount();
    cout << "Outputs: " << num_outputs << '\n';
    for (size_t i = 0; i < num_outputs; ++i) 
    {
        auto name = session_.GetOutputNameAllocated(i, Ort::AllocatorWithDefaultOptions());
        auto type_info = session_.GetOutputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();
        
        cout << "  [" << i << "] " << name.get() << " - Shape: [";
        for (size_t j = 0; j < shape.size(); ++j) {
            cout << shape[j];
            if (j < shape.size() - 1) cout << ", ";
        }
        cout << "]\n";
    }
    cout << "===========================\n\n";
}

