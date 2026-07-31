#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/core/analysis/analyzer_interface.h"
#include "omniface/core/analysis/l2cs/l2cs_estimator.h"
#include "omniface/core/analysis/mediapipe/mediapipe_analyzer.h"
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
    // 统一分析结果（各后端通过 IAnalyzer 接口产出）
    std::vector<analysis::FrameAnalysisResult> analysis_results;
    // 保留原生结果以兼容现有可视化/导出代码（Phase 5-6 清理）
    FaceAnalysisResult analysis;
    std::vector<analysis::mediapipe::MediaPipeAnalysisResult> onnx_results;
    std::vector<analysis::l2cs::L2csEstimationResult> l2cs_results;
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
