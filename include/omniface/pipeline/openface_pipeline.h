#pragma once

#include "omniface/pipeline/base_pipeline.h"

namespace omniface {

class OpenFacePipeline : public BasePipeline {
public:
    using BasePipeline::BasePipeline;

protected:
    void PerformAnalysis(cv::Mat& frame, FrameResult& result) override;
};

}  // namespace omniface
