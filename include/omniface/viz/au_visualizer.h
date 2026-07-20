#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

namespace omniface::analysis {
struct AuAnalysisResult;
}

namespace omniface::viz {

class AuVisualizer {
public:
    static void DrawAuText(cv::Mat& image, const analysis::AuAnalysisResult& result,
                           const cv::Point& position, int& label_y);
};

}  // namespace omniface::viz
