#include "omniface/core/detail/onnx_session.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace omniface::detail {

namespace {
#ifdef _WIN32
std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &result[0], len);
    return result;
}
#endif
}  // namespace

OnnxSession::OnnxSession(const std::string& model_path, const char* log_tag, int intra_threads)
    : memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    , env_(ORT_LOGGING_LEVEL_WARNING, log_tag)
    , opts_([&] {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(intra_threads);
        opts.SetInterOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        opts.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);

#ifdef _WIN32
        OrtOpenVINOProviderOptions ov_opts{};
        ov_opts.device_type = "CPU_FP32";
        try { opts.AppendExecutionProvider_OpenVINO(ov_opts); }
        catch (const Ort::Exception&) {}
        catch (...) {}

        try { opts.AppendExecutionProvider("Dnnl", {}); }
        catch (const Ort::Exception&) {}
        catch (...) {}
#endif
        return opts;
    }())
#ifdef _WIN32
    , session_(env_, ToWide(model_path).c_str(), opts_) {
#else
    , session_(env_, model_path.c_str(), opts_) {
#endif

    Ort::AllocatorWithDefaultOptions allocator;
    size_t num_inputs = session_.GetInputCount();
    for (size_t i = 0; i < num_inputs; ++i) {
        auto name = session_.GetInputNameAllocated(i, allocator);
        input_names_.push_back(name.get());
    }
    for (auto& name : input_names_) {
        input_ptrs_.push_back(name.c_str());
    }

    size_t num_outputs = session_.GetOutputCount();
    for (size_t i = 0; i < num_outputs; ++i) {
        auto name = session_.GetOutputNameAllocated(i, allocator);
        output_names_.push_back(name.get());
    }
    for (auto& name : output_names_) {
        output_ptrs_.push_back(name.c_str());
    }
}

std::vector<Ort::Value> OnnxSession::Run(const float* data, size_t count,
                                         int64_t channels, int64_t height, int64_t width) {
    std::array<int64_t, 4> shape{1, channels, height, width};
    auto input = Ort::Value::CreateTensor<float>(memory_info_, const_cast<float*>(data), count,
                                                  shape.data(), shape.size());
    return session_.Run(Ort::RunOptions{nullptr}, input_ptrs_.data(), &input, 1,
                         output_ptrs_.data(), output_ptrs_.size());
}

std::vector<Ort::Value> OnnxSession::RunBatch(const float* data, size_t count,
                                               int64_t batch, int64_t channels,
                                               int64_t height, int64_t width) {
    std::array<int64_t, 4> shape{batch, channels, height, width};
    auto input = Ort::Value::CreateTensor<float>(memory_info_, const_cast<float*>(data), count,
                                                  shape.data(), shape.size());
    return session_.Run(Ort::RunOptions{nullptr}, input_ptrs_.data(), &input, 1,
                         output_ptrs_.data(), output_ptrs_.size());
}

}  // namespace omniface::detail
