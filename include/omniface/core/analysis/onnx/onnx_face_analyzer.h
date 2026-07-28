#pragma once

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/core/analysis/onnx/pose_estimator.h"
#include "omniface/types.h"

namespace omniface::analysis::onnx {

struct OnnxAnalysisResult {
    PoseEstimationResult pose;
    bool pose_valid = false;
    int track_id = -1;
};

struct OnnxAnalyzerConfig {
    bool enable_pose = true;
    bool enable_gaze = false;
    bool enable_au = false;
    std::string landmark_model;
    std::string canonical_model;
    std::string gaze_model;
    std::string au_model;
    int analysis_threads = 1;
    float margin = 0.25f;
    float score_threshold = 0.95f;
};

class OnnxFaceAnalyzer {
public:
    explicit OnnxFaceAnalyzer(const OnnxAnalyzerConfig& config);
    ~OnnxFaceAnalyzer();

    OnnxFaceAnalyzer(const OnnxFaceAnalyzer&) = delete;
    OnnxFaceAnalyzer& operator=(const OnnxFaceAnalyzer&) = delete;
    OnnxFaceAnalyzer(OnnxFaceAnalyzer&&) noexcept;
    OnnxFaceAnalyzer& operator=(OnnxFaceAnalyzer&&) noexcept;

    // Analyze a single face, returns result with pose estimation
    OnnxAnalysisResult Analyze(const cv::Mat& frame, const BoundingBox& bbox, int track_id = -1);

    // Analyze all tracked faces
    std::vector<OnnxAnalysisResult> AnalyzeBatch(
        const cv::Mat& frame, const std::vector<TrackedFace>& faces);

    // Access the pose estimator (for visualization queries)
    const PoseEstimator* pose_estimator() const { return pose_estimator_.get(); }
    bool has_pose() const { return pose_estimator_ != nullptr; }

private:
    std::unique_ptr<PoseEstimator> pose_estimator_;
    OnnxAnalyzerConfig config_;
};

}  // namespace omniface::analysis::onnx
