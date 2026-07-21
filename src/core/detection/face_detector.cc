#include "omniface/core/detection/face_detector.h"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

#include "omniface/core/detail/onnx_session.h"

namespace omniface::detection {

class FaceDetector::Impl {
public:
    detail::OnnxSession session_;
    Config config_;

    struct AnchorCenters {
        std::vector<float> stride_8;
        std::vector<float> stride_16;
        std::vector<float> stride_32;
    } anchors_;

    Impl(const std::string& model_path, Config cfg)
        : session_(model_path, "FaceDetector", cfg.intra_threads), config_(cfg) {
        anchors_ = GenerateAnchorCenters(config_.input_width, config_.input_height);
    }

private:
    static AnchorCenters GenerateAnchorCenters(int input_w, int input_h) {
        AnchorCenters centers;
        const int strides[] = {8, 16, 32};

        for (int s : strides) {
            int grid_w = input_w / s;
            int grid_h = input_h / s;
            int count = grid_w * grid_h * 2;
            std::vector<float> buf(count * 2);

            size_t idx = 0;
            for (int y = 0; y < grid_h; ++y) {
                for (int x = 0; x < grid_w; ++x) {
                    float cx = static_cast<float>(x * s);
                    float cy = static_cast<float>(y * s);
                    for (int a = 0; a < 2; ++a) {
                        buf[idx++] = cx;
                        buf[idx++] = cy;
                    }
                }
            }

            switch (s) {
                case 8:
                    centers.stride_8 = std::move(buf);
                    break;
                case 16:
                    centers.stride_16 = std::move(buf);
                    break;
                case 32:
                    centers.stride_32 = std::move(buf);
                    break;
            }
        }
        return centers;
    }
};

FaceDetector::FaceDetector(const std::string& model_path, Config config) {
    if (model_path.empty()) throw std::invalid_argument("FaceDetector: model_path is empty");
    impl_ = std::make_unique<Impl>(model_path, config);
}

FaceDetector::~FaceDetector() = default;

namespace {

// SCRFD 预处理：letterbox 缩放 + (pixel-127.5)/128 归一化 + BGR→RGB
std::pair<std::vector<float>, float> PreprocessLetterbox(const cv::Mat& bgr_image,
                                                         const FaceDetector::Config& cfg) {
    float scale = std::min(static_cast<float>(cfg.input_width) / bgr_image.cols,
                           static_cast<float>(cfg.input_height) / bgr_image.rows);

    int new_w = static_cast<int>(bgr_image.cols * scale);
    int new_h = static_cast<int>(bgr_image.rows * scale);

    cv::Mat resized;
    cv::resize(bgr_image, resized, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);

    cv::Mat canvas(cfg.input_height, cfg.input_width, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(canvas(cv::Rect(0, 0, new_w, new_h)));

    cv::Mat rgb_float;
    canvas.convertTo(rgb_float, CV_32FC3, 1.0 / 128.0, -127.5 / 128.0);
    cv::cvtColor(rgb_float, rgb_float, cv::COLOR_BGR2RGB);

    cv::Mat blob_mat =
        cv::dnn::blobFromImage(rgb_float, 1.0, cv::Size(), cv::Scalar(), false, false, CV_32F);
    std::vector<float> blob(blob_mat.ptr<float>(), blob_mat.ptr<float>() + blob_mat.total());

    return {std::move(blob), scale};
}

inline void Distance2Bbox(const float* anchor, const float* bbox_dist, int stride, float& x1,
                          float& y1, float& x2, float& y2) {
    x1 = anchor[0] - bbox_dist[0] * stride;
    y1 = anchor[1] - bbox_dist[1] * stride;
    x2 = anchor[0] + bbox_dist[2] * stride;
    y2 = anchor[1] + bbox_dist[3] * stride;
}

inline void Distance2Kps(const float* anchor, const float* kps_dist, int stride, float* kps_out) {
    for (int k = 0; k < 5; ++k) {
        kps_out[2 * k] = anchor[0] + kps_dist[2 * k] * stride;
        kps_out[2 * k + 1] = anchor[1] + kps_dist[2 * k + 1] * stride;
    }
}

}  // namespace

// SCRFD 检测：三尺度 anchor (stride 8/16/32) 解码 + NMS
std::vector<BoundingBox> FaceDetector::Detect(const cv::Mat& bgr_image) {
    if (bgr_image.empty()) return {};

    auto& cfg = impl_->config_;
    auto [blob, det_scale] = PreprocessLetterbox(bgr_image, cfg);
    std::vector<Ort::Value> outputs;
    try {
        // ONNX 输出: scores_8, scores_16, scores_32, bboxes_8, bboxes_16, bboxes_32, kps_8, kps_16, kps_32
        outputs = impl_->session_.Run(blob, 3, cfg.input_height, cfg.input_width);
    } catch (const std::exception& e) {
        return {};
    }

    struct StrideInfo {
        int stride;
        int count;
        const float* centers;
    };
    const StrideInfo strides[] = {
        {8, 12800, impl_->anchors_.stride_8.data()},
        {16, 3200, impl_->anchors_.stride_16.data()},
        {32, 800, impl_->anchors_.stride_32.data()},
    };

    std::vector<BoundingBox> candidates;
    std::vector<float> confidences;
    std::vector<cv::Rect2d> boxes_rect;

    // 遍历三个 stride 尺度，解码 anchor → bbox + 5点关键点
    for (int i = 0; i < 3; ++i) {
        const auto& s = strides[i];
        const float* scores = outputs[i].GetTensorData<float>();
        const float* bboxes = outputs[i + 3].GetTensorData<float>();
        const float* kps_raw = outputs[i + 6].GetTensorData<float>();

        for (int j = 0; j < s.count; ++j) {
            float score = scores[j];
            if (score < cfg.confidence_threshold) continue;

            const float* anchor = &s.centers[j * 2];
            const float* bbox = &bboxes[j * 4];

            float x1, y1, x2, y2;
            Distance2Bbox(anchor, bbox, s.stride, x1, y1, x2, y2);

            // 映射回原始图像坐标
            x1 /= det_scale;
            y1 /= det_scale;
            x2 /= det_scale;
            y2 /= det_scale;

            x1 = std::max(0.0f, std::min(x1, static_cast<float>(bgr_image.cols - 1)));
            y1 = std::max(0.0f, std::min(y1, static_cast<float>(bgr_image.rows - 1)));
            x2 = std::max(0.0f, std::min(x2, static_cast<float>(bgr_image.cols - 1)));
            y2 = std::max(0.0f, std::min(y2, static_cast<float>(bgr_image.rows - 1)));

            BoundingBox box;
            box.x1 = x1;
            box.y1 = y1;
            box.x2 = x2;
            box.y2 = y2;
            box.confidence = score;

            const float* kps = &kps_raw[j * 10];
            Distance2Kps(anchor, kps, s.stride, box.keypoints.data());
            for (int k = 0; k < 10; ++k) box.keypoints[k] /= det_scale;

            candidates.push_back(box);
            confidences.push_back(score);
            boxes_rect.emplace_back(x1, y1, x2 - x1, y2 - y1);
        }
    }

    std::vector<int> keep;
    if (!candidates.empty())
        cv::dnn::NMSBoxes(boxes_rect, confidences, cfg.confidence_threshold, cfg.nms_threshold,
                          keep);

    std::vector<BoundingBox> result;
    result.reserve(keep.size());
    for (int idx : keep) result.push_back(candidates[idx]);
    return result;
}

}  // namespace omniface::detection
