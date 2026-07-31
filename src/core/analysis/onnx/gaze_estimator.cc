#include "omniface/core/analysis/onnx/gaze_estimator.h"

#include <cmath>
#include <memory>
#include <vector>

#include <opencv2/imgproc.hpp>
#include <onnxruntime_cxx_api.h>

#include <spdlog/spdlog.h>

#include "omniface/core/detail/onnx_session.h"
#include "omniface/core/detail/preprocess.h"

namespace omniface::analysis::onnx {

namespace {

// 对 1D 向量做 Softmax（原地操作）
void Softmax(std::vector<float>& v) {
    float max_val = v[0];
    for (float x : v) max_val = std::max(max_val, x);
    float sum = 0.0f;
    for (float& x : v) {
        x = std::exp(x - max_val);
        sum += x;
    }
    for (float& x : v) x /= sum;
}

// 将 90 类 logits 解码为角度（度）
// 90 个 bin，每个 bin 宽度 4°，范围 [-180°, +180°]
float DecodeAngle(const float* logits, int num_bins) {
    std::vector<float> probs(logits, logits + num_bins);
    Softmax(probs);
    float expected = 0.0f;
    for (int i = 0; i < num_bins; ++i)
        expected += probs[i] * static_cast<float>(i);
    return expected * 4.0f - 180.0f;
}

// 由偏航/俯仰角（弧度）计算 3D 视线单位向量
// 与官方 yakhyo/gaze-estimation 的 gaze_to_3d() 一致：
//   x = -cos(pitch) * sin(yaw), y = -sin(pitch), z = -cos(pitch) * cos(yaw)
// 对应 2D 绘制公式：
//   dx = -length * sin(yaw) * cos(pitch), dy = -length * sin(pitch)
cv::Vec3f GazeTo3D(float yaw_rad, float pitch_rad) {
    float cos_p = std::cos(pitch_rad);
    float sin_p = std::sin(pitch_rad);
    float cos_y = std::cos(yaw_rad);
    float sin_y = std::sin(yaw_rad);
    return cv::Vec3f(-cos_p * sin_y, -sin_p, -cos_p * cos_y);
}

}  // namespace

class GazeEstimator::Impl {
public:
    Config config_;
    detail::OnnxSession session_;
    detail::FaceCropPreprocessor preproc_;
    bool valid_ = false;

    Impl(const Config& cfg)
        : config_(cfg)
        , session_(cfg.model_path, "GazeEstimator", cfg.intra_threads)
        , preproc_(detail::PreprocessConfig{
              .target_size = 448,
              .norm = detail::NormKind::kImageNet,
              .margin = 0.0f
          }) {
        valid_ = true;
        spdlog::info("GazeEstimator: model loaded ({})", cfg.model_path);
    }
};

GazeEstimator::GazeEstimator(const Config& cfg)
    : impl_(std::make_unique<Impl>(cfg)) {}

GazeEstimator::~GazeEstimator() = default;
GazeEstimator::GazeEstimator(GazeEstimator&&) noexcept = default;
GazeEstimator& GazeEstimator::operator=(GazeEstimator&&) noexcept = default;

bool GazeEstimator::IsValid() const { return impl_ && impl_->valid_; }

GazeEstimationResult GazeEstimator::Estimate(const cv::Mat& bgr_frame,
                                               const BoundingBox& face_bbox) {
    GazeEstimationResult result;
    if (!impl_ || !impl_->valid_) return result;
    if (bgr_frame.empty()) return result;
    if (face_bbox.x2 <= face_bbox.x1 || face_bbox.y2 <= face_bbox.y1) return result;

    detail::PreprocessResult prep;
    impl_->preproc_.Process(bgr_frame, face_bbox, prep);

    auto& memory_info = impl_->session_.CpuMemoryInfo();
    std::array<int64_t, 4> shape{1, 3, 448, 448};
    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, prep.blob.ptr<float>(), prep.blob.total(),
        shape.data(), shape.size());

    std::vector<Ort::Value> outputs;
    try {
        outputs = impl_->session_.Session().Run(
            Ort::RunOptions{nullptr},
            impl_->session_.InputNames(), &input_tensor, 1,
            impl_->session_.OutputNames(), impl_->session_.OutputCount());
    } catch (const std::exception& e) {
        spdlog::warn("GazeEstimator: inference failed: {}", e.what());
        return result;
    }

    if (outputs.size() < 2) return result;

    float yaw_deg = DecodeAngle(outputs[0].GetTensorData<float>(), 90);
    float pitch_deg = DecodeAngle(outputs[1].GetTensorData<float>(), 90);

    constexpr float kDegToRad = static_cast<float>(CV_PI) / 180.0f;
    float yaw_rad = yaw_deg * kDegToRad;
    float pitch_rad = pitch_deg * kDegToRad;

    result.yaw = yaw_deg;
    result.pitch = pitch_deg;
    result.gaze_vector = GazeTo3D(yaw_rad, pitch_rad);
    result.valid = true;

    return result;
}

}  // namespace omniface::analysis::onnx
