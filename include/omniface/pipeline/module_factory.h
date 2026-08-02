#pragma once

#include <memory>

#include "omniface/core/analysis/openface/openface_analyzer.h"
#include "omniface/core/analysis/l2cs/l2cs_estimator.h"
#include "omniface/core/analysis/mefarg_au/mefarg_au_estimator.h"
#include "omniface/core/analysis/mediapipe/mediapipe_analyzer.h"
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
    std::unique_ptr<analysis::openface::OpenFaceAnalyzer> face_analyzer;
    std::unique_ptr<analysis::mediapipe::MediaPipeAnalyzer> onnx_analyzer;
    std::unique_ptr<analysis::l2cs::L2csEstimator> l2cs_estimator;
    std::unique_ptr<analysis::mefarg_au::MefargAuEstimator> au_estimator;
};

PipelineModules CreateModules(const PipelineConfig& config);

FaceEmbedding ExtractReferenceEmbedding(const std::string& image_path,
                                        detection::FaceDetector& detector,
                                        recognition::FaceRecognizer& recognizer);

}  // namespace omniface
