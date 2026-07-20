#pragma once

#include <opencv2/core/mat.hpp>
#include <vector>

#include "omniface/types.h"

namespace omniface::viz {

class Visualizer {
public:
    static void DrawAnnotations(cv::Mat& frame, const std::vector<TrackedFace>& tracked_faces,
                                const std::vector<MatchResult>& match_results, int target_track_id);

    static void DrawStatusBar(cv::Mat& frame, int target_track_id);
};

}  // namespace omniface::viz
