#include "omniface/core/analysis/gaze_estimator.h"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

#include "omniface/core/detail/onnx_session.h"
#include "omniface/core/detail/preprocess.h"

namespace omniface::analysis {

namespace {

void Softmax(std::vector<float>& x) {
    size_t n = x.size();
    float max_val = *std::max_element(x.begin(), x.end());
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        x[i] = std::exp(x[i] - max_val);
        sum += x[i];
    }
    for (size_t i = 0; i < n; ++i) {
        x[i] /= sum;
    }
}

float DecodeAngle(const float* logits, int num_bins) {
    std::vector<float> probs(logits, logits + num_bins);
    Softmax(probs);

    float expected = 0.0f;
    for (int i = 0; i < num_bins; ++i) {
        expected += probs[i] * static_cast<float>(i);
    }

    constexpr float kBinWidth = 4.0f;
    constexpr float kAngleOffset = 180.0f;
    float angle_deg = expected * kBinWidth - kAngleOffset;

    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
    return angle_deg * kDegToRad;
}

}  // namespace

class GazeEstimator::Impl {
public:
    detail::OnnxSession session_;
    detail::FaceCropPreprocessor preproc_;
    Config config_;

    Impl(const std::string& model_path, Config cfg)
        : session_(model_path, "GazeEstimator", cfg.intra_threads),
          preproc_(detail::PreprocessConfig{
              .target_size = 448, .norm = detail::NormKind::kImageNet, .margin = cfg.margin}),
          config_(cfg) {}
};

GazeEstimator::GazeEstimator(const std::string& model_path, Config config) {
    if (model_path.empty()) {
        throw std::invalid_argument("GazeEstimator: model_path is empty");
    }
    impl_ = std::make_unique<Impl>(model_path, std::move(config));
}

GazeEstimator::~GazeEstimator() = default;
GazeEstimator::GazeEstimator(GazeEstimator&&) noexcept = default;
GazeEstimator& GazeEstimator::operator=(GazeEstimator&&) noexcept = default;

GazeResult GazeEstimator::Estimate(const cv::Mat& bgr_image, const BoundingBox& face_bbox) {
    GazeResult result;

    if (bgr_image.empty()) return result;
    if (face_bbox.x2 <= face_bbox.x1 || face_bbox.y2 <= face_bbox.y1) return result;

    detail::PreprocessResult prep;
    impl_->preproc_.Process(bgr_image, face_bbox, prep);

    int target = impl_->preproc_.TargetSize();
    std::vector<Ort::Value> outputs;
    try {
        outputs = impl_->session_.Run(prep.blob.ptr<float>(), prep.blob.total(), 3, target, target);
    } catch (const std::exception& e) {
        return result;
    }

    const float* yaw_logits = nullptr;
    const float* pitch_logits = nullptr;

    for (size_t i = 0; i < impl_->session_.OutputCount(); ++i) {
        const auto& name = impl_->session_.OutputNameStrs()[i];
        if (name.find("yaw") != std::string::npos) {
            yaw_logits = outputs[i].GetTensorData<float>();
        } else if (name.find("pitch") != std::string::npos) {
            pitch_logits = outputs[i].GetTensorData<float>();
        }
    }

    if (!yaw_logits || !pitch_logits) {
        return result;
    }

    constexpr int kNumBins = 90;
    result.yaw = DecodeAngle(yaw_logits, kNumBins);
    result.pitch = DecodeAngle(pitch_logits, kNumBins);
    result.valid = true;

    return result;
}

GazeResult GazeEstimator::EstimateFromAligned(const cv::Mat& aligned_face_bgr) {
    GazeResult result;

    if (aligned_face_bgr.empty()) return result;
    if (aligned_face_bgr.cols != 448 || aligned_face_bgr.rows != 448) return result;

    cv::Mat float_img;
    aligned_face_bgr.convertTo(float_img, CV_32FC3, 1.0 / 255.0);

    cv::cvtColor(float_img, float_img, cv::COLOR_BGR2RGB);

    std::vector<cv::Mat> channels(3);
    cv::split(float_img, channels);
    channels[0].convertTo(channels[0], CV_32F, 1.0f / detail::kImageNetStd[0],
                          -detail::kImageNetMean[0] / detail::kImageNetStd[0]);
    channels[1].convertTo(channels[1], CV_32F, 1.0f / detail::kImageNetStd[1],
                          -detail::kImageNetMean[1] / detail::kImageNetStd[1]);
    channels[2].convertTo(channels[2], CV_32F, 1.0f / detail::kImageNetStd[2],
                          -detail::kImageNetMean[2] / detail::kImageNetStd[2]);
    cv::merge(channels, float_img);

    cv::Mat blob_mat =
        cv::dnn::blobFromImage(float_img, 1.0, cv::Size(), cv::Scalar(), false, false, CV_32F);

    std::vector<Ort::Value> outputs;
    try {
        outputs = impl_->session_.Run(blob_mat.ptr<float>(), blob_mat.total(), 3, 448, 448);
    } catch (const std::exception& e) {
        return result;
    }

    const float* yaw_logits = nullptr;
    const float* pitch_logits = nullptr;

    for (size_t i = 0; i < impl_->session_.OutputCount(); ++i) {
        const auto& name = impl_->session_.OutputNameStrs()[i];
        if (name.find("yaw") != std::string::npos) {
            yaw_logits = outputs[i].GetTensorData<float>();
        } else if (name.find("pitch") != std::string::npos) {
            pitch_logits = outputs[i].GetTensorData<float>();
        }
    }

    if (!yaw_logits || !pitch_logits) {
        return result;
    }

    constexpr int kNumBins = 90;
    result.yaw = DecodeAngle(yaw_logits, kNumBins);
    result.pitch = DecodeAngle(pitch_logits, kNumBins);
    result.valid = true;

    return result;
}

}  // namespace omniface::analysis
