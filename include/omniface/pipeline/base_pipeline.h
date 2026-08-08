#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/pipeline/config.h"
#include "omniface/pipeline/module_factory.h"
#include "omniface/pipeline/output_manager.h"
#include "omniface/pipeline/pipeline.h"
#include "omniface/pipeline/target_tracker.h"
#include "omniface/types.h"

namespace omniface {

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
