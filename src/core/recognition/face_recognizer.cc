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

std::vector<FaceEmbedding> FaceRecognizer::ExtractBatch(
    const std::vector<cv::Mat>& aligned_faces) {
    if (aligned_faces.empty()) return {};

    // 单张走原路径，避免批量分配开销
    if (aligned_faces.size() == 1) {
        return {Extract(aligned_faces[0])};
    }

    auto& cfg = impl_->config_;
    int channels = 3;
    int height = cfg.input_height;
    int width = cfg.input_width;
    size_t single_face_size = static_cast<size_t>(channels * height * width);

    // 预分配 batch blob: [N, 3, 112, 112]
    std::vector<float> batch_blob(aligned_faces.size() * single_face_size);

    for (size_t i = 0; i < aligned_faces.size(); ++i) {
        if (aligned_faces[i].empty()) {
            // 空图填零，生成无效 embedding
            std::fill_n(batch_blob.begin() + static_cast<long>(i * single_face_size),
                        single_face_size, 0.0f);
            continue;
        }
        auto blob = Preprocess(aligned_faces[i], cfg);
        std::copy(blob.begin(), blob.end(),
                  batch_blob.begin() + static_cast<long>(i * single_face_size));
    }

    std::vector<FaceEmbedding> results(aligned_faces.size());
    try {
        auto outputs = impl_->session_.RunBatch(
            batch_blob.data(), batch_blob.size(),
            static_cast<int64_t>(aligned_faces.size()), channels, height, width);

        const float* output_data = outputs[0].GetTensorData<float>();
        auto output_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        if (output_shape.size() < 2) return results;
        int dim = static_cast<int>(output_shape[1]);
        if (dim > 512) dim = 512;

        for (size_t i = 0; i < aligned_faces.size(); ++i) {
            std::copy_n(output_data + i * dim, dim, results[i].features.begin());
            results[i].valid = true;
        }
    } catch (const std::exception& e) {
    }
    return results;
}

}  // namespace omniface::recognition
