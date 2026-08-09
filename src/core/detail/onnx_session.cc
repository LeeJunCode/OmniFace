#include "omniface/core/detail/onnx_session.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <spdlog/spdlog.h>

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

bool OnnxSession::CheckProviderAvailable(InferenceDevice device) {
    if (device == InferenceDevice::kCPU) return true;

    try {
        Ort::Env env(ORT_LOGGING_LEVEL_ERROR, "DeviceCheck");
        Ort::SessionOptions opts;
        switch (device) {
        case InferenceDevice::kDirectML:
            opts.AppendExecutionProvider("DML", {});
            break;
        case InferenceDevice::kCUDA: {
            OrtCUDAProviderOptions cuda_opts{};
            cuda_opts.device_id = 0;
            opts.AppendExecutionProvider_CUDA(cuda_opts);
            break;
        }
        case InferenceDevice::kTensorRT:
            opts.AppendExecutionProvider("Tensorrt", {});
            break;
        default:
            break;
        }
        return true;
    } catch (...) {
        return false;
    }
}

void OnnxSession::ConfigureProviders(Ort::SessionOptions& opts, InferenceDevice device,
                                      bool& gpu_available) {
    switch (device) {
    case InferenceDevice::kCPU: {
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
        break;
    }
    case InferenceDevice::kDirectML: {
#ifdef _WIN32
        try {
            opts.AppendExecutionProvider("DML", {});
            gpu_available = true;
            spdlog::info("DirectML execution provider registered");
        } catch (const Ort::Exception&) {
            spdlog::warn("DirectML not available, falling back to CPU");
        } catch (...) {
            spdlog::warn("DirectML not available, falling back to CPU");
        }
#endif
        break;
    }
    case InferenceDevice::kCUDA: {
#ifdef OMNIFACE_HAS_CUDA
        try {
            OrtCUDAProviderOptions cuda_opts{};
            cuda_opts.device_id = 0;
            opts.AppendExecutionProvider_CUDA(cuda_opts);
            gpu_available = true;
            spdlog::info("CUDA execution provider registered");
        } catch (const Ort::Exception&) {
            spdlog::warn("CUDA not available, falling back to CPU");
        } catch (...) {
            spdlog::warn("CUDA not available, falling back to CPU");
        }
#else
        spdlog::warn("CUDA support not compiled in, falling back to CPU");
#endif
        break;
    }
    case InferenceDevice::kTensorRT: {
#ifdef OMNIFACE_HAS_CUDA
        try {
            opts.AppendExecutionProvider("Tensorrt", {});
            gpu_available = true;
            spdlog::info("TensorRT execution provider registered");
        } catch (const Ort::Exception&) {
            spdlog::warn("TensorRT not available, falling back to CPU");
        } catch (...) {
            spdlog::warn("TensorRT not available, falling back to CPU");
        }
#else
        spdlog::warn("TensorRT support not compiled in, falling back to CPU");
#endif
        break;
    }
    }
}

OnnxSession::OnnxSession(const std::string& model_path, const char* log_tag,
                         int intra_threads, InferenceDevice device)
    : memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    , env_(ORT_LOGGING_LEVEL_WARNING, log_tag)
    , device_(device)
    , opts_([&] {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(intra_threads);
        opts.SetInterOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        opts.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);

        bool gpu_available = false;
        ConfigureProviders(opts, device, gpu_available);
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

    std::string names_str;
    for (const auto& n : output_names_) {
        if (!names_str.empty()) names_str += ", ";
        names_str += n;
    }
    spdlog::info("{} ONNX outputs ({}): [{}]", log_tag, output_names_.size(), names_str);
}

std::vector<Ort::Value> OnnxSession::RunWithIoBinding(
    const float* data, size_t count, const std::array<int64_t, 4>& shape) {

    auto input = Ort::Value::CreateTensor<float>(
        memory_info_, const_cast<float*>(data), count, shape.data(), shape.size());

    Ort::IoBinding binding(session_);
    binding.BindInput(input_ptrs_[0], input);

    // Bind outputs with CPU memory info — ORT allocates and handles GPU→CPU copy
    for (size_t i = 0; i < output_ptrs_.size(); ++i) {
        binding.BindOutput(output_ptrs_[i], memory_info_);
    }

    session_.Run(Ort::RunOptions{nullptr}, binding);
    return binding.GetOutputValues();
}

std::vector<Ort::Value> OnnxSession::Run(const float* data, size_t count,
                                         int64_t channels, int64_t height, int64_t width) {
    std::array<int64_t, 4> shape{1, channels, height, width};
    return RunWithIoBinding(data, count, shape);
}

std::vector<Ort::Value> OnnxSession::RunBatch(const float* data, size_t count,
                                               int64_t batch, int64_t channels,
                                               int64_t height, int64_t width) {
    std::array<int64_t, 4> shape{batch, channels, height, width};
    return RunWithIoBinding(data, count, shape);
}

}  // namespace omniface::detail
