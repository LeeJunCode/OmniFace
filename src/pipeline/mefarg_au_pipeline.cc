#include "omniface/pipeline/mefarg_au_pipeline.h"

#include <opencv2/imgproc.hpp>

#include "omniface/core/analysis/analyzer_interface.h"
#include "omniface/core/analysis/mefarg_au/mefarg_au_estimator.h"

namespace omniface {

void MefargAuPipeline::PerformAnalysis(cv::Mat& frame, FrameResult& result) {
    if (!modules_.au_estimator || !modules_.au_estimator->IsValid()
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

    auto au_result = modules_.au_estimator->Estimate(frame, bbox);
    if (au_result.valid) {
        modules_.au_estimator->EnsureCsvOpen(output_mgr_.GetOutputDir());
        double ts = (video_fps_ > 0.0)
            ? (result.frame_index - 1) / video_fps_ : 0.0;
        modules_.au_estimator->WriteCsvRow(result.frame_index, ts, au_result);

        analysis::FrameAnalysisResult unified;
        unified.backend = analysis::AnalyzerKind::kMefargAu;
        unified.track_id = target_tracker_.GetTargetTrackId();
        unified.valid = true;
        unified.action_units.reserve(analysis::mefarg_au::kNumAuOutputs);
        for (int i = 0; i < analysis::mefarg_au::kNumAuOutputs; ++i) {
            unified.action_units.emplace_back(
                analysis::mefarg_au::kAuNames[i],
                static_cast<double>(au_result.au_probs[i]));
        }
        result.analysis_results.push_back(std::move(unified));

        if (config_.mefarg_au.draw_au) {
            analysis::mefarg_au::MefargAuEstimator::DrawAu(
                frame, target_bbox, au_result,
                config_.mefarg_au.threshold);
        }
    }
}

}  // namespace omniface
