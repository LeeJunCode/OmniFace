#include "omniface/core/analysis/l2cs/l2cs_estimator.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include <opencv2/imgproc.hpp>
#include <onnxruntime_cxx_api.h>

#include <spdlog/spdlog.h>

#include "omniface/core/detail/onnx_session.h"
#include "omniface/core/detail/preprocess.h"

namespace omniface::analysis::l2cs {

namespace {

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

float DecodeAngle(const float* logits, int num_bins) {
    std::vector<float> probs(logits, logits + num_bins);
    Softmax(probs);
    float expected = 0.0f;
    for (int i = 0; i < num_bins; ++i)
        expected += probs[i] * static_cast<float>(i);
    return expected * 4.0f - 180.0f;
}

cv::Vec3f GazeTo3D(float yaw_rad, float pitch_rad) {
    float cos_p = std::cos(pitch_rad);
    float sin_p = std::sin(pitch_rad);
    float cos_y = std::cos(yaw_rad);
    float sin_y = std::sin(yaw_rad);
    return cv::Vec3f(-cos_p * sin_y, -sin_p, -cos_p * cos_y);
}

}  // namespace

class L2csEstimator::Impl {
public:
    Config config_;
    detail::OnnxSession session_;
    detail::FaceCropPreprocessor preproc_;
    bool valid_ = false;

    // bbox + gaze EMA 平滑状态
    float smooth_yaw_ = 0.0f;
    float smooth_pitch_ = 0.0f;
    float smooth_bbox_x_ = 0.0f;
    float smooth_bbox_y_ = 0.0f;
    float smooth_bbox_w_ = 0.0f;
    float smooth_bbox_h_ = 0.0f;
    bool smooth_init_ = false;
    int prev_target_id_ = -1;

    // CSV output
    std::ofstream csv_;
    bool csv_initialized_ = false;

    Impl(const Config& cfg)
        : config_(cfg)
        , session_(cfg.model_path, "L2csEstimator", cfg.intra_threads)
        , preproc_(detail::PreprocessConfig{
              .target_size = 448,
              .norm = detail::NormKind::kImageNet,
              .margin = 0.0f
          }) {
        valid_ = true;
        spdlog::info("L2csEstimator: model loaded ({})", cfg.model_path);
    }
};

L2csEstimator::L2csEstimator(const Config& cfg)
    : impl_(std::make_unique<Impl>(cfg)) {}

L2csEstimator::~L2csEstimator() = default;
L2csEstimator::L2csEstimator(L2csEstimator&&) noexcept = default;
L2csEstimator& L2csEstimator::operator=(L2csEstimator&&) noexcept = default;

bool L2csEstimator::IsValid() const { return impl_ && impl_->valid_; }

void L2csEstimator::Reset() {
    impl_->smooth_init_ = false;
    impl_->prev_target_id_ = -1;
}

FrameAnalysisResult L2csEstimator::Analyze(const cv::Mat& frame,
                                            const BoundingBox& bbox,
                                            int track_id) {
    auto native = Estimate(frame, bbox, track_id);

    FrameAnalysisResult result;
    result.backend = AnalyzerKind::kL2cs;
    result.track_id = track_id;
    if (native.valid) {
        result.valid = true;
        result.gaze_yaw = native.yaw;
        result.gaze_pitch = native.pitch;
        result.gaze_vector = native.gaze_vector;
    }
    return result;
}

L2csEstimationResult L2csEstimator::Estimate(const cv::Mat& bgr_frame,
                                               const BoundingBox& face_bbox,
                                               int target_id) {
    L2csEstimationResult result;
    if (!impl_ || !impl_->valid_) return result;
    if (bgr_frame.empty()) return result;
    if (face_bbox.x2 <= face_bbox.x1 || face_bbox.y2 <= face_bbox.y1) return result;

    // 检测目标是否切换
    bool target_changed = (target_id != impl_->prev_target_id_);
    impl_->prev_target_id_ = target_id;

    // bbox EMA 平滑
    float raw_x = face_bbox.x1;
    float raw_y = face_bbox.y1;
    float raw_w = face_bbox.x2 - face_bbox.x1;
    float raw_h = face_bbox.y2 - face_bbox.y1;
    if (!impl_->smooth_init_ || target_changed) {
        impl_->smooth_bbox_x_ = raw_x;
        impl_->smooth_bbox_y_ = raw_y;
        impl_->smooth_bbox_w_ = raw_w;
        impl_->smooth_bbox_h_ = raw_h;
    } else {
        constexpr float kBboxAlpha = 0.25f;
        impl_->smooth_bbox_x_ = kBboxAlpha * raw_x + (1.0f - kBboxAlpha) * impl_->smooth_bbox_x_;
        impl_->smooth_bbox_y_ = kBboxAlpha * raw_y + (1.0f - kBboxAlpha) * impl_->smooth_bbox_y_;
        impl_->smooth_bbox_w_ = kBboxAlpha * raw_w + (1.0f - kBboxAlpha) * impl_->smooth_bbox_w_;
        impl_->smooth_bbox_h_ = kBboxAlpha * raw_h + (1.0f - kBboxAlpha) * impl_->smooth_bbox_h_;
    }

    BoundingBox smooth_bbox;
    smooth_bbox.x1 = impl_->smooth_bbox_x_;
    smooth_bbox.y1 = impl_->smooth_bbox_y_;
    smooth_bbox.x2 = impl_->smooth_bbox_x_ + impl_->smooth_bbox_w_;
    smooth_bbox.y2 = impl_->smooth_bbox_y_ + impl_->smooth_bbox_h_;
    smooth_bbox.confidence = 1.0f;

    detail::PreprocessResult prep;
    impl_->preproc_.Process(bgr_frame, smooth_bbox, prep);

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
        spdlog::warn("L2csEstimator: inference failed: {}", e.what());
        return result;
    }

    if (outputs.size() < 2) return result;

    float yaw_deg = DecodeAngle(outputs[0].GetTensorData<float>(), 90);
    float pitch_deg = DecodeAngle(outputs[1].GetTensorData<float>(), 90);

    // gaze EMA 平滑
    if (!impl_->smooth_init_ || target_changed) {
        impl_->smooth_yaw_ = yaw_deg;
        impl_->smooth_pitch_ = pitch_deg;
        impl_->smooth_init_ = true;
    } else {
        constexpr float kGazeAlpha = 0.2f;
        impl_->smooth_yaw_ = kGazeAlpha * yaw_deg + (1.0f - kGazeAlpha) * impl_->smooth_yaw_;
        impl_->smooth_pitch_ = kGazeAlpha * pitch_deg + (1.0f - kGazeAlpha) * impl_->smooth_pitch_;
    }

    constexpr float kDegToRad = static_cast<float>(CV_PI) / 180.0f;
    float yaw_rad = impl_->smooth_yaw_ * kDegToRad;
    float pitch_rad = impl_->smooth_pitch_ * kDegToRad;

    result.yaw = impl_->smooth_yaw_;
    result.pitch = impl_->smooth_pitch_;
    result.gaze_vector = GazeTo3D(yaw_rad, pitch_rad);
    result.valid = true;

    return result;
}

void L2csEstimator::DrawGaze(cv::Mat& frame,
                              const cv::Rect_<float>& target_bbox,
                              const L2csEstimationResult& result) {
    if (!result.valid) return;

    float cx = target_bbox.x + target_bbox.width / 2.0f;
    float cy = target_bbox.y + target_bbox.height / 2.0f;
    cv::Point2f start(cx, cy);
    float arrow_len = target_bbox.width;

    cv::Point2f end(start.x + result.gaze_vector[0] * arrow_len,
                    start.y + result.gaze_vector[1] * arrow_len);
    cv::circle(frame, start, 4, cv::Scalar(255, 0, 255), -1, cv::LINE_AA);
    cv::arrowedLine(frame, start, end, cv::Scalar(255, 0, 255), 2,
                    cv::LINE_AA, 0, 0.25);

    char l2cs_text[48];
    std::snprintf(l2cs_text, sizeof(l2cs_text),
                  "gaze:%.1f,%.1f", result.yaw, result.pitch);
    cv::putText(frame, l2cs_text,
                cv::Point(static_cast<int>(end.x),
                          static_cast<int>(end.y) - 5),
                cv::FONT_HERSHEY_SIMPLEX, 0.4,
                cv::Scalar(255, 0, 255), 1, cv::LINE_AA);
}

void L2csEstimator::EnsureCsvOpen(const std::string& output_dir) {
    if (impl_->csv_initialized_) return;
    std::string csv_path = output_dir + "/l2cs_analysis.csv";
    impl_->csv_.open(csv_path);
    if (impl_->csv_.is_open()) {
        impl_->csv_ << "frame,timestamp,gaze_yaw,gaze_pitch,gaze_x,gaze_y,gaze_z\n";
        impl_->csv_initialized_ = true;
        spdlog::info("L2CS CSV output: {}", csv_path);
    }
}

void L2csEstimator::WriteCsvRow(int frame_index, double timestamp,
                                 const L2csEstimationResult& result) {
    if (!impl_->csv_.is_open()) return;
    impl_->csv_ << frame_index << "," << timestamp << ","
                << result.yaw << "," << result.pitch << ","
                << result.gaze_vector[0] << ","
                << result.gaze_vector[1] << ","
                << result.gaze_vector[2] << "\n";
}

void L2csEstimator::CloseCsv() {
    if (impl_->csv_.is_open()) impl_->csv_.close();
    impl_->csv_initialized_ = false;
}

}  // namespace omniface::analysis::l2cs
