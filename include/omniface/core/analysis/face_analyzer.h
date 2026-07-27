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
        float min_face_aspect = 0.35f;
        int min_face_width_px = 40;
    };

    explicit FaceAnalyzer(const Config& cfg);
    ~FaceAnalyzer();

    FaceAnalyzer(const FaceAnalyzer&) = delete;
    FaceAnalyzer& operator=(const FaceAnalyzer&) = delete;

    FaceAnalysisResult Analyze(cv::Mat& frame, const cv::Rect_<float>& bbox);

    bool IsValid() const;

    cv::Mat GetAlignedFace() const;

    void GetHOG(cv::Mat_<double>& hog, int& num_rows, int& num_cols) const;

    void Reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis
