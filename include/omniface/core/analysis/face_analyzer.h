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
        float min_face_aspect = 0.35f;   // skip analysis below this aspect ratio
        int min_face_width_px = 40;       // skip analysis below this face width
    };

    explicit FaceAnalyzer(const Config& cfg);
    ~FaceAnalyzer();

    FaceAnalyzer(const FaceAnalyzer&) = delete;
    FaceAnalyzer& operator=(const FaceAnalyzer&) = delete;

    // Run facial analysis and render landmarks/pose/gaze/AUs onto `frame`.
    // `bbox` is the axis-aligned bounding box of the target face.
    FaceAnalysisResult Analyze(cv::Mat& frame, const cv::Rect_<float>& bbox);

    // Return the latest similarity-aligned face (call after Analyze).
    cv::Mat GetAlignedFace() const;

    // Return the latest HOG descriptor (call after Analyze).
    void GetHOG(cv::Mat_<double>& hog, int& num_rows, int& num_cols) const;

    // Reset tracking state (call when switching to a different face).
    void Reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis
