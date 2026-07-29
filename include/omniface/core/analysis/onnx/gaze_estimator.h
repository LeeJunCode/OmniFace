#pragma once

#include <memory>
#include <string>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include "omniface/types.h"

namespace omniface::analysis::onnx {

struct GazeEstimationResult {
    float yaw = 0.0f;          // degrees, positive = subject looking right
    float pitch = 0.0f;        // degrees, positive = subject looking up
    cv::Vec3f gaze_vector;     // 3D unit vector (camera coords: +X right, +Y down, +Z forward)
    bool valid = false;
};

// L2CS-Net gaze estimation using MobileOne-S0 backbone
// Input: face crop RGB 448x448 with ImageNet normalization
// Output: yaw/pitch angles (degrees) + 3D gaze direction vector
class GazeEstimator {
public:
    struct Config {
        std::string model_path;
        int intra_threads = 1;
    };

    explicit GazeEstimator(const Config& cfg);
    ~GazeEstimator();
    GazeEstimator(GazeEstimator&&) noexcept;
    GazeEstimator& operator=(GazeEstimator&&) noexcept;

    GazeEstimator(const GazeEstimator&) = delete;
    GazeEstimator& operator=(const GazeEstimator&) = delete;

    GazeEstimationResult Estimate(const cv::Mat& bgr_frame, const BoundingBox& face_bbox);
    bool IsValid() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis::onnx
