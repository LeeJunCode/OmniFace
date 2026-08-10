#include "omniface/pipeline/openface/openface_pipeline.h"

#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

#include "omniface/core/analysis/interface/analyzer_interface.h"

namespace omniface {

void OpenFacePipeline::PerformAnalysis(cv::Mat& frame, FrameResult& result) {
    if (!modules_.face_analyzer || !modules_.face_analyzer->IsValid()
        || !target_tracker_.HasTarget())
        return;

    cv::Rect_<float> target_bbox = target_tracker_.GetTargetBbox();
    if (target_changed_)
        modules_.face_analyzer->Reset();
    if (target_bbox.width <= 0 || target_bbox.height <= 0)
        return;

    result.analysis = modules_.face_analyzer->Analyze(frame, target_bbox);

    if (result.analysis.valid && !config_.behavior.skip_analysis) {
        modules_.face_analyzer->EnsureWriterInitialized(
            output_mgr_.GetVideoSource(),
            output_mgr_.GetOutputDir(), video_fps_,
            modules_.face_analyzer->CanAlignFace());
        cv::Mat aligned = modules_.face_analyzer->GetAlignedFace();
        modules_.face_analyzer->WriteObservation(
            result.analysis, result.frame_index, video_fps_, aligned);

        analysis::FrameAnalysisResult unified;
        unified.backend = analysis::AnalyzerKind::kOpenFace;
        unified.track_id = target_tracker_.GetTargetTrackId();
        unified.valid = result.analysis.valid;
        unified.head_pose = result.analysis.head_pose;
        unified.gaze_angle_x = result.analysis.gaze_angle_x;
        unified.gaze_angle_y = result.analysis.gaze_angle_y;
        unified.landmarks_2d = result.analysis.landmarks_2d;
        unified.action_units = result.analysis.action_units;
        unified.quality_score = result.analysis.quality_score;
        unified.visible_landmarks = result.analysis.visible_landmarks;
        unified.detection_certainty = result.analysis.detection_certainty;
        unified.detection_success = result.analysis.detection_success;
        unified.gaze_0_x = result.analysis.gaze_0_x;
        unified.gaze_0_y = result.analysis.gaze_0_y;
        unified.gaze_0_z = result.analysis.gaze_0_z;
        unified.gaze_1_x = result.analysis.gaze_1_x;
        unified.gaze_1_y = result.analysis.gaze_1_y;
        unified.gaze_1_z = result.analysis.gaze_1_z;
        unified.pose_Tx = result.analysis.pose_Tx;
        unified.pose_Ty = result.analysis.pose_Ty;
        unified.pose_Tz = result.analysis.pose_Tz;
        unified.pdm_rigid = result.analysis.pdm_rigid;
        unified.pdm_nonrigid = result.analysis.pdm_nonrigid;
        unified.eye_landmarks_2d = result.analysis.eye_landmarks_2d;
        unified.eye_landmarks_3d = result.analysis.eye_landmarks_3d;
        unified.landmarks_3d = result.analysis.landmarks_3d;
        result.analysis_results.push_back(unified);
    }
}

}  // namespace omniface
