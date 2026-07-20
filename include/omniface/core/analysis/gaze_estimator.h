#pragma once

#include <memory>
#include <opencv2/core/mat.hpp>
#include <string>

#include "omniface/types.h"

namespace omniface::analysis {

struct GazeResult {
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool valid = false;
};

class GazeEstimator {
public:
    struct Config {
        float margin = 0.0f;
        int intra_threads = 1;
    };

    explicit GazeEstimator(const std::string& model_path, Config config);
    ~GazeEstimator();
    GazeEstimator(GazeEstimator&&) noexcept;
    GazeEstimator& operator=(GazeEstimator&&) noexcept;

    GazeResult Estimate(const cv::Mat& bgr_image, const BoundingBox& face_bbox);

    GazeResult EstimateFromAligned(const cv::Mat& aligned_face_bgr);

private:
    GazeEstimator(const GazeEstimator&) = delete;
    GazeEstimator& operator=(const GazeEstimator&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis
