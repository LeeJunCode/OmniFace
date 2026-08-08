#pragma once

#include "omniface/pipeline/base_pipeline.h"

namespace omniface {

class MediaPipePipeline : public BasePipeline {
public:
    using BasePipeline::BasePipeline;

protected:
    void PerformAnalysis(cv::Mat& frame, FrameResult& result) override;
    void DrawAnalysisOverlays(cv::Mat& frame, const FrameResult& result) override;
};

}  // namespace omniface
