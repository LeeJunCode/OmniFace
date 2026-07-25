#pragma once

#include <vector>

#include "omniface/core/tracking/bytetrack_engine.h"
#include "omniface/types.h"

namespace omniface::tracking {

class FaceTracker {
public:
    struct Config {
        int track_buffer = 30;
        float high_threshold = 0.5f;
        float match_threshold = 0.5f;
        float match_threshold_second = 0.3f;
        float new_track_threshold = 0.6f;
    };

    FaceTracker();
    explicit FaceTracker(Config config);

    std::vector<TrackedFace> Update(const std::vector<BoundingBox>& detections);
    void Reset();

private:
    ByteTrackEngine engine_;
    std::vector<TrackedFace> last_output_;
};

}  // namespace omniface::tracking
