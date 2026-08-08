#include "omniface/pipeline/mediapipe_pipeline.h"

#include <opencv2/imgproc.hpp>

#include "omniface/core/analysis/analyzer_interface.h"
#include "omniface/core/analysis/mediapipe/mediapipe_analyzer.h"

namespace omniface {

void MediaPipePipeline::PerformAnalysis(cv::Mat& frame, FrameResult& result) {
    if (!modules_.onnx_analyzer || !target_tracker_.HasTarget())
        return;

    cv::Rect_<float> target_bbox = target_tracker_.GetTargetBbox();
    if (target_bbox.width <= 0 || target_bbox.height <= 0)
        return;

    BoundingBox bbox;
    bbox.x1 = target_bbox.x;
    bbox.y1 = target_bbox.y;
    bbox.x2 = target_bbox.x + target_bbox.width;
    bbox.y2 = target_bbox.y + target_bbox.height;
    bbox.confidence = 1.0f;

    auto r = modules_.onnx_analyzer->AnalyzePose(
        frame, bbox, target_tracker_.GetTargetTrackId());

    if (r.pose_valid) {
        modules_.onnx_analyzer->EnsureCsvOpen(output_mgr_.GetOutputDir());
        double ts = (video_fps_ > 0.0)
            ? (result.frame_index - 1) / video_fps_ : 0.0;
        modules_.onnx_analyzer->WriteCsvRow(result.frame_index, ts, r);
    }

    result.onnx_results.push_back(std::move(r));
    if (!result.onnx_results.empty() && result.onnx_results.back().pose_valid) {
        analysis::FrameAnalysisResult unified;
        unified.backend = analysis::AnalyzerKind::kMediaPipe;
        unified.track_id = target_tracker_.GetTargetTrackId();
        unified.valid = true;
        unified.head_pose = result.onnx_results.back().pose.pose;
        unified.landmarks = result.onnx_results.back().pose.landmarks;
        result.analysis_results.push_back(unified);
    }
}

void MediaPipePipeline::DrawAnalysisOverlays(cv::Mat& frame,
                                              const FrameResult& result) {
    if (!config_.onnx.realtime_display || !modules_.onnx_analyzer
        || !modules_.onnx_analyzer->has_pose())
        return;

    for (const auto& r : result.onnx_results) {
        modules_.onnx_analyzer->DrawResults(
            frame, r,
            config_.onnx.draw_landmarks,
            config_.onnx.draw_face_wireframe,
            config_.onnx.draw_face_normal);
    }
}

}  // namespace omniface
