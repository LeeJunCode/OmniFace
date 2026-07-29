#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/core/analysis/onnx/gaze_estimator.h"
#include "omniface/core/analysis/onnx/onnx_face_analyzer.h"
#include "omniface/pipeline/config.h"
#include "omniface/types.h"

namespace omniface {

struct FrameResult {
    cv::Mat annotated_frame;
    int frame_index = 0;
    double total_latency_ms = 0;
    double recognition_latency_ms = 0;
    double analysis_latency_ms = 0;
    int face_count = 0;
    std::vector<TrackedFace> tracked_faces;
    std::vector<MatchResult> match_results;
    FaceAnalysisResult analysis;
    std::vector<analysis::onnx::OnnxAnalysisResult> onnx_results;
    std::vector<analysis::onnx::GazeEstimationResult> gaze_results;
};

using FrameCallback = std::function<void(const FrameResult&)>;

class Pipeline {
public:
    explicit Pipeline(const std::string& cfg_path, FrameCallback callback = nullptr);
    explicit Pipeline(const PipelineConfig& cfg, FrameCallback callback = nullptr);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) noexcept;
    Pipeline& operator=(Pipeline&&) noexcept;

    FrameResult ProcessFrame(cv::Mat& bgr_frame, int frame_index = 0);
    void Run();
    void Stop();
    void OpenVideo(const std::string& path);
    bool PollFrame(cv::Mat& frame);
    bool IsDecodeFinished() const;
    double GetFps() const;
    int GetFrameCount() const;
    std::string GetOutputDir() const;
    void UpdateThresholds(const ThresholdConfig& thresholds);
    void EnrollReference(const FaceEmbedding& embedding);

    const PipelineConfig& Config() const;
    bool HasTarget() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface
