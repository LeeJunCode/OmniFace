#pragma once

#include <memory>
#include <string>

#include <opencv2/core/mat.hpp>

#include "omniface/types.h"

namespace omniface::analysis {

class FaceAnalyzer {
public:
    struct Config {
        std::string model_dir;
    };

    explicit FaceAnalyzer(const Config& cfg);
    ~FaceAnalyzer();

    FaceAnalyzer(const FaceAnalyzer&) = delete;
    FaceAnalyzer& operator=(const FaceAnalyzer&) = delete;

    // Run facial analysis and render landmarks/pose/gaze/AUs onto `frame`.
    // `bbox` is the axis-aligned bounding box of the target face.
    FaceAnalysisResult Analyze(cv::Mat& frame, const cv::Rect_<float>& bbox);

    // Reset tracking state (call when switching to a different face).
    void Reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis
