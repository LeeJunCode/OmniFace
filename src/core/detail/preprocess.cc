#include "omniface/core/detail/preprocess.h"

#include <algorithm>

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

namespace omniface::detail {

FaceCropPreprocessor::FaceCropPreprocessor(PreprocessConfig cfg) : cfg_(std::move(cfg)) {}

FaceCropPreprocessor FaceCropPreprocessor::Clone() const {
    return FaceCropPreprocessor(cfg_);
}

void FaceCropPreprocessor::ProcessFromCrop(const cv::Mat& face_roi_bgr,
                                            const cv::Rect& crop_rect,
                                            PreprocessResult& out) {
    int target = cfg_.target_size;
    out.crop_rect = crop_rect;

    // 归一化参数: 三种模式对应不同模型的训练预处理
    // kInsightFaceDet: [0,255] → ~[-1,1]  (用于SCRFD检测)
    // kInsightFaceRec: [0,255] → [-1,1]   (用于ArcFace识别)
    // 默认:            [0,255] → [0,1]    (通用)
    float alpha = 1.0f;
    float beta = 0.0f;
    switch (cfg_.norm) {
    case NormKind::kInsightFaceDet:
        alpha = 1.0f / 128.0f; beta = -127.5f / 128.0f; break;
    case NormKind::kInsightFaceRec:
        alpha = 1.0f / 127.5f; beta = -1.0f; break;
    default:
        alpha = 1.0f / 255.0f; beta = 0.0f; break;
    }

    if (cfg_.letterbox) {
        float scale = std::min(
            static_cast<float>(target) / face_roi_bgr.cols,
            static_cast<float>(target) / face_roi_bgr.rows);
        int new_w = static_cast<int>(face_roi_bgr.cols * scale);
        int new_h = static_cast<int>(face_roi_bgr.rows * scale);

        cv::resize(face_roi_bgr, resized_, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);
        canvas_.create(target, target, CV_8UC3);
        canvas_.setTo(cfg_.letterbox_fill);
        resized_.copyTo(canvas_(cv::Rect(0, 0, new_w, new_h)));
        canvas_.convertTo(float_img_, CV_32FC3, alpha, beta);
    } else {
        cv::resize(face_roi_bgr, resized_, cv::Size(target, target), 0, 0, cv::INTER_LINEAR);
        resized_.convertTo(float_img_, CV_32FC3, alpha, beta);
    }

    // 转换为RGB通道顺序 (OpenCV默认BGR)
    cv::cvtColor(float_img_, float_img_, cv::COLOR_BGR2RGB);

    // ImageNet标准化: 每个通道独立减均值除标准差 (适用于部分分类模型)
    if (cfg_.norm == NormKind::kImageNet) {
        std::vector<cv::Mat> channels(3);
        cv::split(float_img_, channels);
        channels[0].convertTo(channels[0], CV_32F, 1.0f / kImageNetStd[0], -kImageNetMean[0] / kImageNetStd[0]);
        channels[1].convertTo(channels[1], CV_32F, 1.0f / kImageNetStd[1], -kImageNetMean[1] / kImageNetStd[1]);
        channels[2].convertTo(channels[2], CV_32F, 1.0f / kImageNetStd[2], -kImageNetMean[2] / kImageNetStd[2]);
        cv::merge(channels, float_img_);
    }

    out.blob = cv::dnn::blobFromImage(float_img_, 1.0, cv::Size(), cv::Scalar(), false, false, CV_32F).clone();
}

void FaceCropPreprocessor::Process(const cv::Mat& bgr_image, const BoundingBox& face_bbox,
                                    PreprocessResult& out) {
    int img_w = bgr_image.cols;
    int img_h = bgr_image.rows;
    int target = cfg_.target_size;

    float bw = face_bbox.x2 - face_bbox.x1;
    float bh = face_bbox.y2 - face_bbox.y1;
    // 按边界框尺寸比例扩展边距，为对齐/识别保留更多上下文信息
    float mx = bw * cfg_.margin;
    float my = bh * cfg_.margin;

    int cx1 = std::max(0, static_cast<int>(face_bbox.x1 - mx));
    int cy1 = std::max(0, static_cast<int>(face_bbox.y1 - my));
    int cx2 = std::min(img_w, static_cast<int>(face_bbox.x2 + mx));
    int cy2 = std::min(img_h, static_cast<int>(face_bbox.y2 + my));

    cv::Rect crop_rect(cx1, cy1, cx2 - cx1, cy2 - cy1);
    cv::Mat face_roi = bgr_image(crop_rect);
    ProcessFromCrop(face_roi, crop_rect, out);
}

}  // namespace omniface::detail
