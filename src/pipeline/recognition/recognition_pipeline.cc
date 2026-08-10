#include "omniface/pipeline/recognition/recognition_pipeline.h"

namespace omniface {

void RecognitionPipeline::PerformAnalysis(cv::Mat& /*frame*/, FrameResult& /*result*/) {
    // "None" backend: detection + tracking + recognition only, no analysis.
}

}  // namespace omniface
