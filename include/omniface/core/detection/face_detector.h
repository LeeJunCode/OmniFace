#pragma once

#include <memory>
#include <opencv2/core/mat.hpp>
#include <string>
#include <vector>

#include "omniface/types.h"

namespace omniface::detection {

class FaceDetector {
public:
    struct Config {
        float nms_threshold = 0.4f;
        float confidence_threshold = 0.5f;
        int input_width = 640;
        int input_height = 640;
        int intra_threads = 2;
    };

    explicit FaceDetector(const std::string& model_path, Config config);
    ~FaceDetector();

    std::vector<BoundingBox> Detect(const cv::Mat& bgr_image);

private:
    FaceDetector(const FaceDetector&) = delete;
    FaceDetector& operator=(const FaceDetector&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::detection
