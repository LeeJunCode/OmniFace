#include "omniface/pipeline/l2cs/l2cs_pipeline.h"

#include <opencv2/imgproc.hpp>

#include "omniface/core/analysis/interface/analyzer_interface.h"
#include "omniface/core/analysis/l2cs/l2cs_estimator.h"

namespace omniface {

void L2csPipeline::PerformAnalysis(cv::Mat& frame, FrameResult& result) {
    if (!modules_.l2cs_estimator || !modules_.l2cs_estimator->IsValid()
        || !target_tracker_.HasTarget())
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

    int cur_target_id = target_tracker_.GetTargetTrackId();
    auto l2cs_result = modules_.l2cs_estimator->Estimate(frame, bbox, cur_target_id);

    if (l2cs_result.valid) {
        modules_.l2cs_estimator->EnsureCsvOpen(output_mgr_.GetOutputDir());
        double ts = (video_fps_ > 0.0)
            ? (result.frame_index - 1) / video_fps_ : 0.0;
        modules_.l2cs_estimator->WriteCsvRow(result.frame_index, ts, l2cs_result);
    }

    result.l2cs_results.push_back(std::move(l2cs_result));
    if (!result.l2cs_results.empty() && result.l2cs_results.back().valid) {
        analysis::FrameAnalysisResult unified;
        unified.backend = analysis::AnalyzerKind::kL2cs;
        unified.track_id = target_tracker_.GetTargetTrackId();
        unified.valid = true;
        unified.gaze_yaw = result.l2cs_results.back().yaw;
        unified.gaze_pitch = result.l2cs_results.back().pitch;
        unified.gaze_vector = result.l2cs_results.back().gaze_vector;
        result.analysis_results.push_back(unified);
    }
}

void L2csPipeline::DrawAnalysisOverlays(cv::Mat& frame,
                                         const FrameResult& result) {
    if (!config_.l2cs.draw_gaze || result.l2cs_results.empty()
        || !target_tracker_.HasTarget())
        return;

    cv::Rect_<float> cur_bbox = target_tracker_.GetTargetBbox();
    for (const auto& gr : result.l2cs_results) {
        analysis::l2cs::L2csEstimator::DrawGaze(frame, cur_bbox, gr);
    }
}

}  // namespace omniface
