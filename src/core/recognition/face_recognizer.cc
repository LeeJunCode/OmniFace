#include "omniface/core/recognition/face_recognizer.h"

#include <onnxruntime_cxx_api.h>

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

#include "omniface/core/detail/onnx_session.h"

namespace omniface::recognition {

class FaceRecognizer::Impl {
public:
    detail::OnnxSession session_;
    Config config_;

    Impl(const std::string& model_path, Config cfg)
        : session_(model_path, "FaceRecognizer", cfg.intra_threads), config_(cfg) {}
};

FaceRecognizer::FaceRecognizer(const std::string& model_path, Config config) {
    if (model_path.empty()) {
        throw std::invalid_argument("FaceRecognizer: model_path is empty");
    }
    impl_ = std::make_unique<Impl>(model_path, config);
}

FaceRecognizer::~FaceRecognizer() = default;

namespace {

std::vector<float> Preprocess(const cv::Mat& bgr_image, const FaceRecognizer::Config& cfg) {
    cv::Mat resized;
    cv::resize(bgr_image, resized, cv::Size(cfg.input_width, cfg.input_height), 0, 0,
               cv::INTER_LINEAR);

    cv::Mat float_img;
    resized.convertTo(float_img, CV_32FC3, 1.0 / 127.5, -1.0);

    cv::cvtColor(float_img, float_img, cv::COLOR_BGR2RGB);

    cv::Mat blob_mat =
        cv::dnn::blobFromImage(float_img, 1.0, cv::Size(), cv::Scalar(), false, false, CV_32F);
    std::vector<float> blob(blob_mat.ptr<float>(), blob_mat.ptr<float>() + blob_mat.total());

    return blob;
}

}  // namespace

FaceEmbedding FaceRecognizer::Extract(const cv::Mat& aligned_face_bgr) {
    if (aligned_face_bgr.empty()) {
        return {};
    }

    auto blob = Preprocess(aligned_face_bgr, impl_->config_);

    FaceEmbedding embedding;
    try {
        auto& cfg = impl_->config_;
        auto outputs = impl_->session_.Run(blob, 3, cfg.input_height, cfg.input_width);

        const float* output_data = outputs[0].GetTensorData<float>();
        auto output_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        if (output_shape.size() < 2) return {};
        int dim = static_cast<int>(output_shape[1]);
        if (dim > 512) dim = 512;

        std::copy_n(output_data, dim, embedding.features.begin());
        embedding.valid = true;
    } catch (const std::exception& e) {
    }
    return embedding;
}

}  // namespace omniface::recognition
