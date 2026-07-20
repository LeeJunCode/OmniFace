#pragma once

#include <memory>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <string>
#include <vector>

#include "omniface/types.h"

namespace omniface::analysis {

struct PoseEstimationResult {
    HeadPose pose;
    cv::Point2f nose_tip;
    std::vector<cv::Point2f> landmarks;
    cv::Mat rvec;
    cv::Mat tvec;
    cv::Mat camera_matrix;
    bool valid = false;
};

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

    PoseEstimationResult EstimatePose(const cv::Mat& bgr_image, const BoundingBox& face_bbox);

    const std::vector<cv::Point3f>& canonical_model() const;
    float face_size() const;

private:
    PoseEstimator(const PoseEstimator&) = delete;
    PoseEstimator& operator=(const PoseEstimator&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis
