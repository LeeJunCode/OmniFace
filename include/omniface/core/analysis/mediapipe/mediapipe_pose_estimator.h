#pragma once

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include "omniface/core/inference/onnx_session.h"
#include "omniface/types.h"

namespace omniface::analysis::mediapipe {

struct PoseEstimationResult {
    HeadPose pose;
    cv::Point2f nose_tip;
    std::vector<cv::Point2f> landmarks;
    cv::Mat rvec;
    cv::Mat tvec;
    cv::Mat camera_matrix;
    float score = 0.0f;
    float reproj_rms = 0.0f;
    float nose_proj_err = 0.0f;
    bool valid = false;
};

class MediaPipePoseEstimator {
public:
    struct Config {
        float margin = 0.25f;
        float score_threshold = 0.95f;
        std::string canonical_model_path;
        int intra_threads = 1;
        InferenceDevice device = InferenceDevice::kCPU;
    };

    explicit MediaPipePoseEstimator(const std::string& landmark_model_path, Config config);
    ~MediaPipePoseEstimator();
    MediaPipePoseEstimator(MediaPipePoseEstimator&&) noexcept;
    MediaPipePoseEstimator& operator=(MediaPipePoseEstimator&&) noexcept;

    PoseEstimationResult EstimatePose(const cv::Mat& bgr_image,
                                      const BoundingBox& face_bbox);

    PoseEstimationResult EstimatePoseFromCrop(const cv::Mat& face_roi,
                                              const cv::Rect& crop_rect,
                                              int original_img_w = 0,
                                              int original_img_h = 0);

    MediaPipePoseEstimator Clone() const;

    const std::vector<cv::Point3f>& canonical_model() const;
    float face_size() const;

private:
    MediaPipePoseEstimator(const MediaPipePoseEstimator&) = delete;
    MediaPipePoseEstimator& operator=(const MediaPipePoseEstimator&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis::mediapipe
