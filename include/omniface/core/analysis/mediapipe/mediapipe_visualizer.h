#pragma once

#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

namespace omniface::analysis::mediapipe {

struct PoseEstimationResult;

class MediaPipeVisualizer {
public:
    static void DrawLandmarks(cv::Mat& image,
                              const std::vector<cv::Point2f>& landmarks);

    static void DrawProjectedModel(cv::Mat& image,
                                   const PoseEstimationResult& result,
                                   const std::vector<cv::Point3f>& canonical_model);

    static void DrawFaceNormal(cv::Mat& image,
                               const PoseEstimationResult& result,
                               float face_size,
                               const cv::Point3f& nose_canonical);
};

}  // namespace omniface::analysis::mediapipe
