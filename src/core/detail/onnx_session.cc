#include "omniface/core/detail/onnx_session.h"

namespace omniface::detail {

// ONNX 会话封装：统一管理内存、选项、输入输出名称
OnnxSession::OnnxSession(const std::string& model_path, const char* log_tag, int intra_threads)
    : memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      env_(ORT_LOGGING_LEVEL_WARNING, log_tag),
      opts_([&] {
          Ort::SessionOptions opts;
          opts.SetIntraOpNumThreads(intra_threads);
          opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
          opts.SetExecutionMode(ExecutionMode::ORT_PARALLEL);
          opts.EnableCpuMemArena();
          opts.EnableMemPattern();
          return opts;
      }()),
      session_(env_, model_path.c_str(), opts_) {
    size_t num_inputs = session_.GetInputCount();
    for (size_t i = 0; i < num_inputs; ++i) {
        auto name = session_.GetInputNameAllocated(i, Ort::AllocatorWithDefaultOptions{});
        input_names_.push_back(name.get());
    }
    for (auto& name : input_names_) {
        input_ptrs_.push_back(name.c_str());
    }

    size_t num_outputs = session_.GetOutputCount();
    for (size_t i = 0; i < num_outputs; ++i) {
        auto name = session_.GetOutputNameAllocated(i, Ort::AllocatorWithDefaultOptions{});
        output_names_.push_back(name.get());
    }
    for (auto& name : output_names_) {
        output_ptrs_.push_back(name.c_str());
    }
}

std::vector<Ort::Value> OnnxSession::Run(const float* data, size_t count, int64_t channels,
                                         int64_t height, int64_t width) {
    std::array<int64_t, 4> shape{1, channels, height, width};
    auto input = Ort::Value::CreateTensor<float>(memory_info_, const_cast<float*>(data), count,
                                                 shape.data(), shape.size());
    static const Ort::RunOptions run_opts{nullptr};
    return session_.Run(run_opts, input_ptrs_.data(), &input, 1, output_ptrs_.data(),
                        output_ptrs_.size());
}

std::vector<Ort::Value> OnnxSession::RunBatch(const float* data, size_t count, int64_t batch,
                                              int64_t channels, int64_t height, int64_t width) {
    std::array<int64_t, 4> shape{batch, channels, height, width};
    auto input = Ort::Value::CreateTensor<float>(memory_info_, const_cast<float*>(data), count,
                                                 shape.data(), shape.size());
    static const Ort::RunOptions run_opts{nullptr};
    return session_.Run(run_opts, input_ptrs_.data(), &input, 1, output_ptrs_.data(),
                        output_ptrs_.size());
}
}  // namespace omniface::detail
