#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <vector>

namespace omniface::analysis {
struct PoseEstimationResult;
}

namespace omniface::viz {

class PoseVisualizer {
public:
    static void DrawLandmarks(cv::Mat& image, const std::vector<cv::Point2f>& landmarks);

    static void DrawPoseAxes(cv::Mat& image, const analysis::PoseEstimationResult& result,
                             const cv::Point3f& nose_canonical);

    static void DrawProjectedModel(cv::Mat& image, const analysis::PoseEstimationResult& result,
                                   const std::vector<cv::Point3f>& canonical_model);

    static void DrawFaceNormal(cv::Mat& image, const analysis::PoseEstimationResult& result,
                               float face_size, const cv::Point3f& nose_canonical);
};

}  // namespace omniface::viz
