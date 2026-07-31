#pragma once

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include "omniface/types.h"

namespace omniface::analysis::onnx {

struct PoseEstimationResult {
    HeadPose pose;
    cv::Point2f nose_tip;
    std::vector<cv::Point2f> landmarks;     // 468 2D keypoints in original image coords
    cv::Mat rvec;                           // rotation vector (3x1 CV_64F)
    cv::Mat tvec;                           // translation vector (3x1 CV_64F)
    cv::Mat camera_matrix;                  // camera intrinsics (3x3 CV_64F)
    bool valid = false;
};

// MediaPipe FaceMesh 关键点 + PnP 解算头部姿态
class PoseEstimator {
public:
    struct Config {
        float margin = 0.25f;
        float score_threshold = 0.95f;
        std::string canonical_model_path;
        int intra_threads = 1;
    };

    explicit PoseEstimator(const std::string& landmark_model_path, Config config);
    ~PoseEstimator();
    PoseEstimator(PoseEstimator&&) noexcept;
    PoseEstimator& operator=(PoseEstimator&&) noexcept;

    PoseEstimationResult EstimatePose(const cv::Mat& bgr_image,
                                      const BoundingBox& face_bbox);

    PoseEstimationResult EstimatePoseFromCrop(const cv::Mat& face_roi,
                                              const cv::Rect& crop_rect,
                                              int original_img_w = 0,
                                              int original_img_h = 0);

    PoseEstimator Clone() const;

    const std::vector<cv::Point3f>& canonical_model() const;
    float face_size() const;

private:
    PoseEstimator(const PoseEstimator&) = delete;
    PoseEstimator& operator=(const PoseEstimator&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis::onnx
