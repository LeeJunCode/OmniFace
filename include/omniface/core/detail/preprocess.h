#pragma once

#include <opencv2/core/mat.hpp>
#include <vector>

#include "omniface/types.h"

namespace omniface::detail {

enum class NormKind {
    kInsightFaceDet,
    kInsightFaceRec,
    kSimple,
    kImageNet,
};

struct PreprocessConfig {
    int target_size = 192;
    NormKind norm = NormKind::kSimple;
    float margin = 0.0f;
    bool letterbox = false;
    cv::Scalar letterbox_fill{114, 114, 114};
};

struct PreprocessResult {
    cv::Mat blob;
    cv::Rect crop_rect;
};

class FaceCropPreprocessor {
public:
    explicit FaceCropPreprocessor(PreprocessConfig cfg);

    void Process(const cv::Mat& bgr_image, const BoundingBox& face_bbox, PreprocessResult& out);

    int TargetSize() const {
        return cfg_.target_size;
    }
    float Margin() const {
        return cfg_.margin;
    }

private:
    PreprocessConfig cfg_;

    cv::Mat canvas_;
    cv::Mat resized_;
    cv::Mat float_img_;
};

inline constexpr float kImageNetMean[3] = {0.485f, 0.456f, 0.406f};
inline constexpr float kImageNetStd[3] = {0.229f, 0.224f, 0.225f};

}  // namespace omniface::detail
