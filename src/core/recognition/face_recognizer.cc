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
    cv::resize(bgr_image, resized, cv::Size(cfg.input_width, cfg.input_height),
               0, 0, cv::INTER_LINEAR);

    int H = cfg.input_height, W = cfg.input_width;
    // 输出布局: BGR平面分离 — R_plane | G_plane | B_plane (各H*W)
    std::vector<float> blob(static_cast<size_t>(H) * W * 3);
    float* R_plane = blob.data();
    float* G_plane = blob.data() + static_cast<size_t>(H) * W;
    float* B_plane = blob.data() + static_cast<size_t>(H) * W * 2;

    // InsightFace识别归一化: 将[0,255] uint8线性映射到[-1,1]范围
    constexpr float kScale = 1.0f / 127.5f;
    constexpr float kOffset = -1.0f;

    for (int h = 0; h < H; ++h) {
        const uint8_t* row = resized.ptr<uint8_t>(h);
        size_t base = static_cast<size_t>(h) * W;
        for (int w = 0; w < W; ++w) {
            float b = static_cast<float>(row[w * 3 + 0]) * kScale + kOffset;
            float g = static_cast<float>(row[w * 3 + 1]) * kScale + kOffset;
            float r = static_cast<float>(row[w * 3 + 2]) * kScale + kOffset;
            size_t idx = base + w;
            R_plane[idx] = r;
            G_plane[idx] = g;
            B_plane[idx] = b;
        }
    }
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
        // 输入格式: NHWC (通道数3, BGR平面分离)
        auto outputs = impl_->session_.Run(blob, 3, cfg.input_height, cfg.input_width);

        const float* output_data = outputs[0].GetTensorData<float>();
        auto output_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        if (output_shape.size() < 2) return {};
        int dim = static_cast<int>(output_shape[1]);
        // 截断到512维: 部分模型可能输出更高维度，但系统固定使用512维特征
        if (dim > 512) dim = 512;

        std::copy_n(output_data, dim, embedding.features.begin());
        embedding.valid = true;
    } catch (const std::exception&) {
    }
    return embedding;
}

std::vector<FaceEmbedding> FaceRecognizer::ExtractBatch(
    const std::vector<cv::Mat>& aligned_faces) {
    if (aligned_faces.empty()) return {};

    // 单张人脸快速路径: 避免批量拼接和内存拷贝的开销
    if (aligned_faces.size() == 1) {
        return {Extract(aligned_faces[0])};
    }

    auto& cfg = impl_->config_;
    int H = cfg.input_height;
    int W = cfg.input_width;
    size_t batch_size = aligned_faces.size();
    size_t single_face_size = static_cast<size_t>(3) * H * W;

    std::vector<float> batch_blob(batch_size * single_face_size);

    constexpr float kScale = 1.0f / 127.5f;
    constexpr float kOffset = -1.0f;

    for (size_t b = 0; b < batch_size; ++b) {
        float* base = batch_blob.data() + b * single_face_size;
        if (aligned_faces[b].empty()) {
            std::fill_n(base, single_face_size, 0.0f);
            continue;
        }

        cv::Mat resized;
        cv::resize(aligned_faces[b], resized, cv::Size(W, H), 0, 0, cv::INTER_LINEAR);

        float* R_plane = base;
        float* G_plane = base + static_cast<size_t>(H) * W;
        float* B_plane = base + static_cast<size_t>(H) * W * 2;

        for (int h = 0; h < H; ++h) {
            const uint8_t* row = resized.ptr<uint8_t>(h);
            size_t row_base = static_cast<size_t>(h) * W;
            for (int w = 0; w < W; ++w) {
                float b_val = static_cast<float>(row[w * 3 + 0]) * kScale + kOffset;
                float g_val = static_cast<float>(row[w * 3 + 1]) * kScale + kOffset;
                float r_val = static_cast<float>(row[w * 3 + 2]) * kScale + kOffset;
                size_t idx = row_base + w;
                R_plane[idx] = r_val;
                G_plane[idx] = g_val;
                B_plane[idx] = b_val;
            }
        }
    }

    std::vector<FaceEmbedding> results(aligned_faces.size());
    try {
        auto outputs = impl_->session_.RunBatch(
            batch_blob.data(), batch_blob.size(),
            static_cast<int64_t>(batch_size), 3, H, W);

        const float* output_data = outputs[0].GetTensorData<float>();
        auto output_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        if (output_shape.size() < 2) return results;
        int dim = static_cast<int>(output_shape[1]);
        // 截断到512维
        if (dim > 512) dim = 512;

        for (size_t i = 0; i < aligned_faces.size(); ++i) {
            std::copy_n(output_data + i * dim, dim, results[i].features.begin());
            results[i].valid = true;
        }
    } catch (const std::exception&) {
    }
    return results;
}

}  // namespace omniface::recognition
