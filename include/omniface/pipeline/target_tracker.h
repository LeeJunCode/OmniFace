#pragma once

#include <unordered_map>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/core/recognition/face_matcher.h"
#include "omniface/core/recognition/face_recognizer.h"
#include "omniface/pipeline/config.h"
#include "omniface/types.h"

namespace omniface {

class TargetTracker {
public:
    explicit TargetTracker(const PipelineConfig& config);

    void ProcessMatches(const cv::Mat& frame,
                        std::vector<BoundingBox>& detections,
                        std::vector<TrackedFace>& tracked_faces,
                        recognition::FaceRecognizer* recognizer,
                        recognition::FaceMatcher* matcher,
                        std::vector<MatchResult>& out_matches,
                        int& out_best_match_idx,
                        float& out_best_match_sim);

    bool UpdateTargetState(const std::vector<TrackedFace>& tracked_faces,
                           const std::vector<MatchResult>& match_results,
                           int best_match_idx, float best_match_sim);

    int GetTargetTrackId() const { return target_track_id_; }
    cv::Rect_<float> GetTargetBbox() const { return last_target_bbox_; }
    bool HasTarget() const { return target_track_id_ > 0; }

    void ResetAnalysisState() { target_verify_fails_ = 0; target_demote_ = false; }

    void SetFrameIndex(int idx) { frame_index_ = idx; }

private:
    struct TrackInfo {
        bool is_target = false;
        float similarity = 0.0f;
    };

    void PruneTrackCache(const std::vector<TrackedFace>& active_faces);

    PipelineConfig config_;
    std::unordered_map<int, TrackInfo> track_cache_;
    int target_track_id_ = -1;
    int target_verify_fails_ = 0;
    bool target_demote_ = false;
    int last_target_verify_frame_ = 0;
    cv::Rect_<float> last_target_bbox_;
    int frame_index_ = 0;
};

}  // namespace omniface
