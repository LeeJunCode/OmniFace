#pragma once

#include <memory>
#include <string>

#include "omniface/pipeline/framework/base_pipeline.h"
#include "omniface/pipeline/framework/config.h"
#include "omniface/types.h"

namespace omniface {

std::unique_ptr<BasePipeline> CreatePipeline(
    AnalysisBackend backend,
    const PipelineConfig& config,
    FaceEmbedding reference,
    FrameCallback callback = nullptr);

FaceEmbedding ExtractReferenceEmbedding(const std::string& image_path,
                                        detection::FaceDetector& detector,
                                        recognition::FaceRecognizer& recognizer);

}  // namespace omniface
