#include "omniface/core/analysis/au_analyzer.h"

#include <onnxruntime_cxx_api.h>

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

#include "omniface/core/detail/onnx_session.h"
#include "omniface/core/detail/preprocess.h"

namespace omniface::analysis {

class AuAnalyzer::Impl {
public:
    detail::OnnxSession session_;
    detail::FaceCropPreprocessor preproc_;
    Config config_;

    Impl(const std::string& model_path, Config cfg)
        : session_(model_path, "AuAnalyzer", cfg.intra_threads),
          preproc_(detail::PreprocessConfig{
              .target_size = 224, .norm = detail::NormKind::kImageNet, .margin = cfg.margin}),
          config_(cfg) {
        if (session_.OutputCount() != 1) {
            throw std::runtime_error("AuAnalyzer: expected 1 output tensor, got " +
                                     std::to_string(session_.OutputCount()));
        }
    }
};

AuAnalyzer::AuAnalyzer(const std::string& model_path, Config config) {
    if (model_path.empty()) {
        throw std::invalid_argument("AuAnalyzer: model_path is empty");
    }
    impl_ = std::make_unique<Impl>(model_path, std::move(config));
}

AuAnalyzer::~AuAnalyzer() = default;
AuAnalyzer::AuAnalyzer(AuAnalyzer&&) noexcept = default;
AuAnalyzer& AuAnalyzer::operator=(AuAnalyzer&&) noexcept = default;

AuAnalysisResult AuAnalyzer::Analyze(const cv::Mat& bgr_image, const BoundingBox& face_bbox) {
    AuAnalysisResult result;

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

    auto output_info = outputs[0].GetTensorTypeAndShapeInfo();
    auto output_shape = output_info.GetShape();
    if (output_shape.size() != 2 || output_shape[0] != 1 || output_shape[1] != 41) {
        return result;
    }
    const float* logits = outputs[0].GetTensorData<float>();

    const float kDefaultThreshold = impl_->config_.threshold;
    const auto& per_au = impl_->config_.per_au_thresholds;

    for (int i = 0; i < 27; ++i) {
        result.main_aus[i].cosine_sim = logits[i];
        float thr = kDefaultThreshold;
        auto it = per_au.find(std::string(kMainAuNames[i]));
        if (it != per_au.end()) thr = it->second;
        result.main_aus[i].active = (logits[i] >= thr);
    }
    for (int i = 0; i < 14; ++i) {
        result.sub_aus[i].cosine_sim = logits[27 + i];
        float thr = kDefaultThreshold;
        auto it = per_au.find(std::string(kSubAuNames[i]));
        if (it != per_au.end()) thr = it->second;
        result.sub_aus[i].active = (logits[27 + i] >= thr);
    }
    result.valid = true;

    return result;
}

AuAnalysisResult AuAnalyzer::AnalyzeFromAligned(const cv::Mat& aligned_face_bgr) {
    AuAnalysisResult result;

    if (aligned_face_bgr.empty()) return result;
    if (aligned_face_bgr.cols != 224 || aligned_face_bgr.rows != 224) return result;

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
        outputs = impl_->session_.Run(blob_mat.ptr<float>(), blob_mat.total(), 3, 224, 224);
    } catch (const std::exception& e) {
        return result;
    }

    auto output_info = outputs[0].GetTensorTypeAndShapeInfo();
    auto output_shape = output_info.GetShape();
    if (output_shape.size() != 2 || output_shape[0] != 1 || output_shape[1] != 41) {
        return result;
    }
    const float* logits = outputs[0].GetTensorData<float>();

    const float kDefaultThreshold = impl_->config_.threshold;
    const auto& per_au = impl_->config_.per_au_thresholds;

    for (int i = 0; i < 27; ++i) {
        result.main_aus[i].cosine_sim = logits[i];
        float thr = kDefaultThreshold;
        auto it = per_au.find(std::string(kMainAuNames[i]));
        if (it != per_au.end()) thr = it->second;
        result.main_aus[i].active = (logits[i] >= thr);
    }
    for (int i = 0; i < 14; ++i) {
        result.sub_aus[i].cosine_sim = logits[27 + i];
        float thr = kDefaultThreshold;
        auto it = per_au.find(std::string(kSubAuNames[i]));
        if (it != per_au.end()) thr = it->second;
        result.sub_aus[i].active = (logits[27 + i] >= thr);
    }
    result.valid = true;

    return result;
}

}  // namespace omniface::analysis
