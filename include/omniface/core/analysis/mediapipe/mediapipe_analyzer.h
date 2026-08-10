#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/core/analysis/interface/analyzer_interface.h"
#include "omniface/core/analysis/mediapipe/mediapipe_pose_estimator.h"
#include "omniface/core/inference/onnx_session.h"
#include "omniface/types.h"

namespace omniface::analysis::mediapipe {

struct MediaPipeAnalysisResult {
    PoseEstimationResult pose;
    bool pose_valid = false;
    int track_id = -1;
};

struct MediaPipeAnalyzerConfig {
    bool enable_pose = true;
    bool enable_au = false;
    std::string landmark_model;
    std::string canonical_model;
    std::string au_model;
    int analysis_threads = 1;
    float margin = 0.25f;
    float score_threshold = 0.95f;
    InferenceDevice device = InferenceDevice::kCPU;
};

class MediaPipeAnalyzer : public IAnalyzer {
public:
    explicit MediaPipeAnalyzer(const MediaPipeAnalyzerConfig& config);
    ~MediaPipeAnalyzer();

    MediaPipeAnalyzer(const MediaPipeAnalyzer&) = delete;
    MediaPipeAnalyzer& operator=(const MediaPipeAnalyzer&) = delete;
    MediaPipeAnalyzer(MediaPipeAnalyzer&&) noexcept;
    MediaPipeAnalyzer& operator=(MediaPipeAnalyzer&&) noexcept;

    // IAnalyzer interface
    AnalyzerKind Kind() const override { return AnalyzerKind::kMediaPipe; }
    FrameAnalysisResult Analyze(const cv::Mat& frame, const BoundingBox& bbox,
                                int track_id = -1) override;
    bool IsValid() const override { return has_pose(); }
    void Reset() override {}

    // MediaPipe native interface (full PoseEstimationResult with rvec/tvec/camera_matrix)
    MediaPipeAnalysisResult AnalyzePose(const cv::Mat& frame, const BoundingBox& bbox, int track_id = -1);

    std::vector<MediaPipeAnalysisResult> AnalyzeBatch(
        const cv::Mat& frame, const std::vector<TrackedFace>& faces);

    const MediaPipePoseEstimator* pose_estimator() const { return pose_estimator_.get(); }
    bool has_pose() const { return pose_estimator_ != nullptr; }

    // Frame visualization (draws landmarks, wireframe, face normal, pose text)
    void DrawResults(cv::Mat& frame,
                     const MediaPipeAnalysisResult& result,
                     bool draw_landmarks,
                     bool draw_face_wireframe,
                     bool draw_face_normal);

    // CSV output (lazy init)
    void EnsureCsvOpen(const std::string& output_dir);
    void WriteCsvRow(int frame_index, double timestamp,
                     const MediaPipeAnalysisResult& result);
    void CloseCsv();

private:
    std::unique_ptr<MediaPipePoseEstimator> pose_estimator_;
    MediaPipeAnalyzerConfig config_;
    std::ofstream csv_;
    bool csv_initialized_ = false;
};

}  // namespace omniface::analysis::mediapipe
