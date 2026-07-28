#pragma once

#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

namespace omniface::analysis::onnx {

struct PoseEstimationResult;

// Static visualization utilities for head pose estimation results.
// Decoupled from the estimator — operates purely on result structs.
class PoseVisualizer {
public:
    // Draw 468 face mesh landmarks as green dots
    static void DrawLandmarks(cv::Mat& image,
                              const std::vector<cv::Point2f>& landmarks);

    // Draw projected 3D face wireframe (face oval, eyes, eyebrows, nose, lips)
    static void DrawProjectedModel(cv::Mat& image,
                                   const PoseEstimationResult& result,
                                   const std::vector<cv::Point3f>& canonical_model);

    // Draw face normal arrow (magenta) from nose tip
    static void DrawFaceNormal(cv::Mat& image,
                               const PoseEstimationResult& result,
                               float face_size,
                               const cv::Point3f& nose_canonical);
};

}  // namespace omniface::analysis::onnx
