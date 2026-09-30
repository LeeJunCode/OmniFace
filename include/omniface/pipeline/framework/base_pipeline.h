#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/core/analysis/interface/analyzer_interface.h"
#include "omniface/core/analysis/l2cs/l2cs_estimator.h"
#include "omniface/core/analysis/mediapipe/mediapipe_analyzer.h"
#include "omniface/pipeline/framework/config.h"
#include "omniface/pipeline/framework/module_factory.h"
#include "omniface/pipeline/workers/output_manager.h"
#include "omniface/pipeline/workers/target_tracker.h"
#include "omniface/types.h"

namespace omniface {

struct FrameResult {
    cv::Mat annotated_frame;
    int frame_index = 0;
    double total_latency_ms = 0;
    // 检测 + 跟踪 + 识别三段之和（既有含义，保持不变）
    double recognition_latency_ms = 0;
    double analysis_latency_ms = 0;
    // 分阶段耗时：检测（含下采样回映射）/ 跟踪 / 识别（对齐 + 批量前向 + 匹配）
    double detect_latency_ms = 0;
    double track_latency_ms = 0;
    double match_latency_ms = 0;
    // 本帧送进识别网络前向的样本数
    int recognition_calls = 0;
    int face_count = 0;
    std::vector<TrackedFace> tracked_faces;
    std::vector<MatchResult> match_results;
    std::vector<analysis::FrameAnalysisResult> analysis_results;
    // 保留原生结果以兼容现有可视化/导出代码
    FaceAnalysisResult analysis;
    std::vector<analysis::mediapipe::MediaPipeAnalysisResult> onnx_results;
    std::vector<analysis::l2cs::L2csEstimationResult> l2cs_results;
};

using FrameCallback = std::function<void(const FrameResult&)>;

class BasePipeline {
public:
    BasePipeline(const PipelineConfig& config, FaceEmbedding reference,
                 FrameCallback callback = nullptr);
    virtual ~BasePipeline();

    BasePipeline(const BasePipeline&) = delete;
    BasePipeline& operator=(const BasePipeline&) = delete;
    BasePipeline(BasePipeline&&) noexcept;
    BasePipeline& operator=(BasePipeline&&) noexcept;

    FrameResult ProcessFrame(cv::Mat& bgr_frame, int frame_index);
    void EnrollReference(const FaceEmbedding& embedding);
    void UpdateThresholds(const ThresholdConfig& thresholds);
    void SetVideoSource(const std::string& video_path);
    void SetVideoFps(double fps) { video_fps_ = fps; }
    void FinalizeOutput() { output_mgr_.Finalize(); }

    const PipelineConfig& Config() const { return config_; }
    bool HasTarget() const { return target_tracker_.HasTarget(); }
    std::string OutputDir() const { return output_mgr_.GetOutputDir(); }
    std::string OutputVideoPath() const { return output_mgr_.GetOutputVideoPath(); }

protected:
    virtual void PerformAnalysis(cv::Mat& frame, FrameResult& result) = 0;
    virtual void DrawAnalysisOverlays(cv::Mat& frame, const FrameResult& result) {}

    static void DrawFaceResults(cv::Mat& image,
                                const std::vector<TrackedFace>& tracked_faces,
                                const std::vector<MatchResult>& matches,
                                int target_track_id);

    PipelineConfig config_;
    FrameCallback callback_;
    PipelineModules modules_;
    OutputManager output_mgr_;
    TargetTracker target_tracker_;
    double video_fps_ = 24.0;
    bool target_changed_ = false;

private:
    void EnsureOutputInitialized(const cv::Size& frame_size);

    bool output_initialized_ = false;
};

}  // namespace omniface
