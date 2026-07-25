#include "omniface/core/tracking/face_tracker.h"

namespace omniface::tracking {

FaceTracker::FaceTracker() : FaceTracker(Config{}) {}

FaceTracker::FaceTracker(Config config)
    : engine_(ByteTrackEngine::Config{
          .track_buffer = config.track_buffer,
          .track_thresh = config.high_threshold,
          .high_thresh = config.new_track_threshold,
          .match_thresh = config.match_threshold,
          .match_thresh_second = config.match_threshold_second,
      }) {}

std::vector<TrackedFace> FaceTracker::Update(const std::vector<BoundingBox>& detections) {
    if (detections.empty()) {
        return last_tracks_;
    }

    std::vector<ByteTrackEngine::Detection> engine_dets;
    engine_dets.reserve(detections.size());
    for (const auto& d : detections) {
        ByteTrackEngine::Detection det;
        det.tlwh[0] = d.x1;
        det.tlwh[1] = d.y1;
        det.tlwh[2] = d.x2 - d.x1;
        det.tlwh[3] = d.y2 - d.y1;
        det.score = d.confidence;
        engine_dets.push_back(det);
    }

    auto tracks = engine_.Update(engine_dets);

    std::vector<TrackedFace> result;
    result.reserve(tracks.size());
    for (const auto& t : tracks) {
        TrackedFace tf;
        tf.id = t.id;
        tf.box.x1 = t.tlwh[0];
        tf.box.y1 = t.tlwh[1];
        tf.box.x2 = t.tlwh[0] + t.tlwh[2];
        tf.box.y2 = t.tlwh[1] + t.tlwh[3];
        tf.box.confidence = t.score;
        tf.consecutive_hit_count = t.hit_streak;
        tf.consecutive_miss_count = t.miss_streak;
        result.push_back(tf);
    }
    last_tracks_ = result;
    return result;
}

void FaceTracker::Reset() {
    engine_.Reset();
    last_tracks_.clear();
}

}  // namespace omniface::tracking
