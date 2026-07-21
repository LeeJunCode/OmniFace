#pragma once

#include <onnxruntime_cxx_api.h>

#include <memory>
#include <string>
#include <vector>

namespace omniface::detail {

class OnnxSession {
public:
    explicit OnnxSession(const std::string& model_path, const char* log_tag, int intra_threads = 4);

    OnnxSession(const OnnxSession&) = delete;
    OnnxSession& operator=(const OnnxSession&) = delete;

    OnnxSession(OnnxSession&&) = default;
    OnnxSession& operator=(OnnxSession&&) = default;

    std::vector<Ort::Value> Run(const float* data, size_t count, int64_t channels, int64_t height,
                                int64_t width);

    std::vector<Ort::Value> Run(const std::vector<float>& input_blob, int64_t channels,
                                int64_t height, int64_t width) {
        return Run(input_blob.data(), input_blob.size(), channels, height, width);
    }

    std::vector<Ort::Value> RunBatch(const float* data, size_t count, int64_t batch,
                                     int64_t channels, int64_t height, int64_t width);

    Ort::Session& Session() {
        return session_;
    }

    const char* const* InputNames() const {
        return input_ptrs_.data();
    }
    const char* const* OutputNames() const {
        return output_ptrs_.data();
    }
    size_t InputCount() const {
        return input_ptrs_.size();
    }
    size_t OutputCount() const {
        return output_ptrs_.size();
    }

    const std::vector<std::string>& OutputNameStrs() const {
        return output_names_;
    }

    const Ort::MemoryInfo& CpuMemoryInfo() const {
        return memory_info_;
    }

private:
    Ort::MemoryInfo memory_info_;
    Ort::Env env_;
    Ort::SessionOptions opts_;
    Ort::Session session_;

    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<const char*> input_ptrs_;
    std::vector<const char*> output_ptrs_;
};

}  // namespace omniface::detail
