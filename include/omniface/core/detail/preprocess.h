#pragma once

#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/types.h"

namespace omniface::detail {

enum class NormKind {
    kInsightFaceDet,   // (pixel - 127.5) / 128.0
    kInsightFaceRec,   // (pixel - 127.5) / 127.5
    kSimple,           // pixel / 255.0
    kImageNet,         // per-channel (pixel - mean) / std
};

struct PreprocessConfig {
    int target_size = 192;
    NormKind norm = NormKind::kSimple;
    float margin = 0.0f;
    bool letterbox = false;
    cv::Scalar letterbox_fill{114, 114, 114};
    bool skip_crop = false;
};

struct PreprocessResult {
    cv::Mat blob;        // CV_32FC1, NCHW layout, continuous
    cv::Rect crop_rect;  // crop region in original image
};

// ImageNet normalization constants
inline constexpr float kImageNetMean[3] = {0.485f, 0.456f, 0.406f};
inline constexpr float kImageNetStd[3]  = {0.229f, 0.224f, 0.225f};

class FaceCropPreprocessor {
public:
    explicit FaceCropPreprocessor(PreprocessConfig cfg);

    void Process(const cv::Mat& bgr_image, const BoundingBox& face_bbox,
                 PreprocessResult& out);

    void ProcessFromCrop(const cv::Mat& face_roi_bgr, const cv::Rect& crop_rect,
                         PreprocessResult& out);

    FaceCropPreprocessor Clone() const;

    int TargetSize() const { return cfg_.target_size; }
    float Margin() const { return cfg_.margin; }

private:
    PreprocessConfig cfg_;
    cv::Mat canvas_;
    cv::Mat resized_;
    cv::Mat float_img_;
};

}  // namespace omniface::detail
