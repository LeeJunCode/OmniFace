#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <vector>

namespace omniface::viz {

class GazeVisualizer {
public:
    static void DrawGazeVectors(cv::Mat& image, const std::vector<cv::Point2f>& landmarks,
                                float pitch_rad, float yaw_rad);

    static void DrawGazeArrow(cv::Mat& image, float x1, float y1, float x2, float y2,
                              float pitch_rad, float yaw_rad);
};

}  // namespace omniface::viz
