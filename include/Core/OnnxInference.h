#ifndef ONNX_INFERENCE_H
#define ONNX_INFERENCE_H

#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>

class DeepFilterNet {
   
public:
    DeepFilterNet(const std::string& model_path);
    ~DeepFilterNet();

    std::vector<float> ApplyNoiseSuppression(const std::vector<float>& audio);
    std::vector<float> ProcessRealtimeFrame(const std::vector<float>& frame); // ADD THIS
    void reset();
    void SetNoiseSuppressionStrength(float db); 
    void PrintModelSummary() const; 
    
private:  
    const int HOP_SIZE = 480;
    const int FFT_SIZE = 960;
    const int STATE_SIZE = 45304;
    
    Ort::Env env_;
    Ort::SessionOptions session_options_;
    Ort::Session session_;
    Ort::MemoryInfo memory_info_;
    Ort::AllocatorWithDefaultOptions allocator;
    
    std::vector<float> state_;
    float atten_lim_db_;
    
    std::vector<float> GetPaddedAudio(const std::vector<float>& audio);
    std::vector<float> GetEnhancedFrame(const std::vector<float>& frame);
    std::vector<float> GetTrimmedOutput(const std::vector<float>& enhanced, int orig_len);
};

#endif