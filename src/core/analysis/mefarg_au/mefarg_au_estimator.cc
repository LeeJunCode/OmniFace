#include "omniface/core/analysis/mefarg_au/mefarg_au_estimator.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <vector>

#include <opencv2/imgproc.hpp>
#include <onnxruntime_cxx_api.h>

#include <spdlog/spdlog.h>

#include "omniface/core/inference/onnx_session.h"

namespace omniface::analysis::mefarg_au {

namespace {

constexpr int kInputSize = 224;
constexpr int kResizeSize = 256;

// ImageNet 归一化参数
constexpr float kMeanR = 0.485f;
constexpr float kMeanG = 0.456f;
constexpr float kMeanB = 0.406f;
constexpr float kStdR = 0.229f;
constexpr float kStdG = 0.224f;
constexpr float kStdB = 0.225f;

// 预处理: 人脸裁剪 → resize(256) → centerCrop(224) → BGR→RGB → ImageNet normalize → CHW
std::vector<float> PreprocessFace(const cv::Mat& bgr_frame,
                                   const BoundingBox& bbox) {
    // 裁剪人脸区域
    int x1 = std::max(0, static_cast<int>(bbox.x1));
    int y1 = std::max(0, static_cast<int>(bbox.y1));
    int x2 = std::min(bgr_frame.cols - 1, static_cast<int>(bbox.x2));
    int y2 = std::min(bgr_frame.rows - 1, static_cast<int>(bbox.y2));
    if (x2 <= x1 || y2 <= y1) return {};

    cv::Mat face = bgr_frame(cv::Rect(x1, y1, x2 - x1, y2 - y1));

    // resize 短边 256，保持宽高比
    int w = face.cols;
    int h = face.rows;
    float scale = static_cast<float>(kResizeSize) / static_cast<float>(std::min(w, h));
    int new_w = static_cast<int>(w * scale);
    int new_h = static_cast<int>(h * scale);
    cv::Mat resized;
    cv::resize(face, resized, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);

    // center crop 224x224
    int cx = (new_w - kInputSize) / 2;
    int cy = (new_h - kInputSize) / 2;
    cx = std::max(0, cx);
    cy = std::max(0, cy);
    cv::Rect crop_rect(cx, cy, kInputSize, kInputSize);
    cv::Mat cropped = resized(crop_rect).clone();

    // BGR → RGB → float32 [0,1]
    cv::Mat rgb;
    cv::cvtColor(cropped, rgb, cv::COLOR_BGR2RGB);
    cv::Mat img_float;
    rgb.convertTo(img_float, CV_32FC3, 1.0 / 255.0);

    // Normalize + CHW: (3, 224, 224)
    std::vector<float> blob(3 * kInputSize * kInputSize);
    for (int c = 0; c < 3; ++c) {
        float mean, std_val;
        if (c == 0)      { mean = kMeanR; std_val = kStdR; }  // R
        else if (c == 1) { mean = kMeanG; std_val = kStdG; }  // G
        else             { mean = kMeanB; std_val = kStdB; }  // B
        for (int row = 0; row < kInputSize; ++row) {
            const float* src = img_float.ptr<float>(row);
            float* dst = blob.data() + c * kInputSize * kInputSize + row * kInputSize;
            for (int col = 0; col < kInputSize; ++col) {
                dst[col] = (src[col * 3 + c] - mean) / std_val;
            }
        }
    }
    return blob;
}

}  // namespace

class MefargAuEstimator::Impl {
public:
    Config config_;
    detail::OnnxSession session_;
    bool valid_ = false;
    std::ofstream csv_;
    bool csv_initialized_ = false;

    explicit Impl(const Config& cfg)
        : config_(cfg)
        , session_(cfg.model, "MefargAuEstimator", cfg.intra_threads, cfg.device) {
        valid_ = true;
        spdlog::info("MefargAuEstimator: model loaded ({}), threshold={:.2f}",
                     cfg.model, cfg.threshold);
    }
};

MefargAuEstimator::MefargAuEstimator(const Config& cfg)
    : impl_(std::make_unique<Impl>(cfg)) {}

MefargAuEstimator::~MefargAuEstimator() = default;
MefargAuEstimator::MefargAuEstimator(MefargAuEstimator&&) noexcept = default;
MefargAuEstimator& MefargAuEstimator::operator=(MefargAuEstimator&&) noexcept = default;

bool MefargAuEstimator::IsValid() const { return impl_ && impl_->valid_; }

FrameAnalysisResult MefargAuEstimator::Analyze(const cv::Mat& frame,
                                                const BoundingBox& bbox,
                                                int track_id) {
    auto native = Estimate(frame, bbox);

    FrameAnalysisResult result;
    result.backend = AnalyzerKind::kMefargAu;
    result.track_id = track_id;
    if (native.valid) {
        result.valid = true;
        result.action_units.reserve(kNumAuOutputs);
        for (int i = 0; i < kNumAuOutputs; ++i) {
            result.action_units.emplace_back(kAuNames[i],
                                              static_cast<double>(native.au_probs[i]));
        }
    }
    return result;
}

MefargAuEstimationResult MefargAuEstimator::Estimate(const cv::Mat& bgr_frame,
                                                       const BoundingBox& face_bbox) {
    MefargAuEstimationResult result;
    if (!impl_ || !impl_->valid_) return result;
    if (bgr_frame.empty()) return result;
    if (face_bbox.x2 <= face_bbox.x1 || face_bbox.y2 <= face_bbox.y1) return result;

    std::vector<float> blob = PreprocessFace(bgr_frame, face_bbox);
    if (blob.empty()) return result;

    auto& memory_info = impl_->session_.CpuMemoryInfo();
    std::array<int64_t, 4> shape{1, 3, kInputSize, kInputSize};
    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, blob.data(), blob.size(), shape.data(), shape.size());

    std::vector<Ort::Value> outputs;
    try {
        outputs = impl_->session_.Session().Run(
            Ort::RunOptions{nullptr},
            impl_->session_.InputNames(), &input_tensor, 1,
            impl_->session_.OutputNames(), impl_->session_.OutputCount());
    } catch (const std::exception& e) {
        spdlog::warn("MefargAuEstimator: inference failed: {}", e.what());
        return result;
    }

    if (outputs.empty()) return result;

    const float* logits = outputs[0].GetTensorData<float>();
    int num_outputs = static_cast<int>(outputs[0].GetTensorTypeAndShapeInfo().GetElementCount());
    if (num_outputs != kNumAuOutputs) {
        spdlog::warn("MefargAuEstimator: unexpected output count {}, expected {}",
                     num_outputs, kNumAuOutputs);
        return result;
    }

    float raw_min = logits[0], raw_max = logits[0];
    int active_count = 0;
    for (int i = 0; i < kNumAuOutputs; ++i) {
        result.au_logits[i] = logits[i];
        // 模型输出是余弦相似度（范围 [-1,1]），训练时直接作为概率使用
        // 不需要 sigmoid，只需 clamp 到 [0,1]
        result.au_probs[i] = std::clamp(logits[i], 0.0f, 1.0f);
        result.au_active[i] = (result.au_probs[i] >= impl_->config_.threshold);
        if (result.au_active[i]) ++active_count;
        if (logits[i] < raw_min) raw_min = logits[i];
        if (logits[i] > raw_max) raw_max = logits[i];
    }
    result.valid = true;

    static int debug_frame_count = 0;
    if (++debug_frame_count % 30 == 1) {
        spdlog::info("MEFARG AU raw output: range=[{:.3f}, {:.3f}], active={}/41",
                     raw_min, raw_max, active_count);
    }

    return result;
}

void MefargAuEstimator::EnsureCsvOpen(const std::string& output_dir) {
    if (impl_->csv_initialized_) return;
    std::string csv_path = output_dir + "/mefarg_au_analysis.csv";
    impl_->csv_.open(csv_path);
    if (impl_->csv_.is_open()) {
        impl_->csv_ << "frame,timestamp";
        for (int i = 0; i < kNumAuOutputs; ++i) {
            impl_->csv_ << "," << kAuNames[i] << "_prob"
                        << "," << kAuNames[i] << "_active";
        }
        impl_->csv_ << "\n";
        impl_->csv_initialized_ = true;
        spdlog::info("MEFARG AU CSV output: {}", csv_path);
    }
}

void MefargAuEstimator::WriteCsvRow(int frame_index, double timestamp,
                                     const MefargAuEstimationResult& result) {
    if (!impl_->csv_.is_open()) return;
    impl_->csv_ << frame_index << "," << timestamp;
    for (int i = 0; i < kNumAuOutputs; ++i) {
        impl_->csv_ << "," << result.au_probs[i]
                    << "," << (result.au_active[i] ? 1 : 0);
    }
    impl_->csv_ << "\n";
}

void MefargAuEstimator::CloseCsv() {
    if (impl_->csv_.is_open()) impl_->csv_.close();
    impl_->csv_initialized_ = false;
}

void MefargAuEstimator::DrawAu(cv::Mat& frame,
                                const cv::Rect_<float>& target_bbox,
                                const MefargAuEstimationResult& result,
                                float threshold) {
    if (!result.valid) return;

    static const std::unordered_map<std::string, std::string> kAuShortDesc = {
        {"AU1", "Inner Brow"}, {"AU2", "Outer Brow"}, {"AU4", "Brow Lower"},
        {"AU5", "Upper Lid"}, {"AU6", "Cheek Raise"}, {"AU7", "Lid Tight"},
        {"AU9", "Nose Wrinkle"}, {"AU10", "Upper Lip Raise"},
        {"AU11", "Nasolabial"}, {"AU12", "Lip Corner"}, {"AU13", "Cheek Puff"},
        {"AU14", "Dimpler"}, {"AU15", "Lip Depress"}, {"AU16", "Lower Lip"},
        {"AU17", "Chin Raise"}, {"AU18", "Lip Pucker"}, {"AU19", "Tongue Show"},
        {"AU20", "Lip Stretch"}, {"AU22", "Lip Funnel"}, {"AU23", "Lip Tight"},
        {"AU24", "Lip Press"}, {"AU25", "Lips Part"}, {"AU26", "Jaw Drop"},
        {"AU27", "Mouth Stretch"}, {"AU32", "Lip Bite"}, {"AU38", "Nostril Dilate"},
        {"AU39", "Nostril Compress"},
        {"AUL1", "Inner Brow L"}, {"AUR1", "Inner Brow R"},
        {"AUL2", "Outer Brow L"}, {"AUR2", "Outer Brow R"},
        {"AUL4", "Brow Lower L"}, {"AUR4", "Brow Lower R"},
        {"AUL6", "Cheek Raise L"}, {"AUR6", "Cheek Raise R"},
        {"AUL10", "Up Lip Raise L"}, {"AUR10", "Up Lip Raise R"},
        {"AUL12", "Nasolabial L"}, {"AUR12", "Nasolabial R"},
        {"AUL14", "Dimpler L"}, {"AUR14", "Dimpler R"},
    };

    int x = static_cast<int>(target_bbox.x + target_bbox.width + 8);
    int y = static_cast<int>(target_bbox.y);
    int line_h = 16;

    // 半透明背景
    int line_count = 0;
    for (int i = 0; i < kNumAuOutputs; ++i) {
        if (result.au_probs[i] >= threshold) ++line_count;
    }
    if (line_count == 0) return;

    cv::Rect bg_rect(x - 4, y - 4, 200, line_count * line_h + 10);
    bg_rect &= cv::Rect(0, 0, frame.cols, frame.rows);
    cv::Mat roi = frame(bg_rect);
    cv::Mat overlay;
    cv::cvtColor(roi, overlay, cv::COLOR_BGR2BGRA);
    cv::Mat bg(overlay.size(), CV_8UC4, cv::Scalar(0, 0, 0, 140));
    cv::addWeighted(overlay, 0.5, bg, 0.5, 0, roi);

    for (int i = 0; i < kNumAuOutputs; ++i) {
        if (result.au_probs[i] < threshold) continue;

        auto desc_it = kAuShortDesc.find(kAuNames[i]);
        std::string desc = (desc_it != kAuShortDesc.end())
            ? desc_it->second : kAuNames[i];

        char label[64];
        std::snprintf(label, sizeof(label), "%s %s: %.2f",
                      kAuNames[i], desc.c_str(), result.au_probs[i]);

        cv::putText(frame, label,
                    cv::Point(x, y + line_h - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    cv::Scalar(0, 255, 100), 1, cv::LINE_AA);
        y += line_h;
    }
}

}  // namespace omniface::analysis::mefarg_au
