#pragma once

#include "omniface/pipeline/framework/base_pipeline.h"

namespace omniface {

class L2csPipeline : public BasePipeline {
public:
    using BasePipeline::BasePipeline;

protected:
    void PerformAnalysis(cv::Mat& frame, FrameResult& result) override;
    void DrawAnalysisOverlays(cv::Mat& frame, const FrameResult& result) override;
};

}  // namespace omniface
