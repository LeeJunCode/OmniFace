#pragma once

#include <memory>

#include "omniface/core/analysis/face_analyzer.h"
#include "omniface/core/analysis/onnx/onnx_face_analyzer.h"
#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/face_matcher.h"
#include "omniface/core/recognition/face_recognizer.h"
#include "omniface/core/tracking/face_tracker.h"
#include "omniface/pipeline/config.h"

namespace omniface {

struct PipelineModules {
    std::unique_ptr<detection::FaceDetector> detector;
    std::unique_ptr<tracking::FaceTracker> tracker;
    std::unique_ptr<recognition::FaceRecognizer> recognizer;
    std::unique_ptr<recognition::FaceMatcher> matcher;
    std::unique_ptr<analysis::FaceAnalyzer> face_analyzer;
    std::unique_ptr<analysis::onnx::OnnxFaceAnalyzer> onnx_analyzer;
};

PipelineModules CreateModules(const PipelineConfig& config);

FaceEmbedding ExtractReferenceEmbedding(const std::string& image_path,
                                        detection::FaceDetector& detector,
                                        recognition::FaceRecognizer& recognizer);

}  // namespace omniface
