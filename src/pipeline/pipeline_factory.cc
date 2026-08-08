#include "omniface/pipeline/pipeline_factory.h"

#include <stdexcept>

#include <opencv2/imgcodecs.hpp>

#include <spdlog/spdlog.h>

#include "omniface/core/recognition/align.h"
#include "omniface/pipeline/l2cs_pipeline.h"
#include "omniface/pipeline/mediapipe_pipeline.h"
#include "omniface/pipeline/mefarg_au_pipeline.h"
#include "omniface/pipeline/openface_pipeline.h"
#include "omniface/pipeline/recognition_pipeline.h"

namespace omniface {

std::unique_ptr<BasePipeline> CreatePipeline(
    AnalysisBackend backend,
    const PipelineConfig& config,
    FaceEmbedding reference,
    FrameCallback callback) {
    switch (backend) {
    case AnalysisBackend::kOpenFace:
        return std::make_unique<OpenFacePipeline>(config, std::move(reference),
                                                   std::move(callback));
    case AnalysisBackend::kMediaPipe:
        return std::make_unique<MediaPipePipeline>(config, std::move(reference),
                                                    std::move(callback));
    case AnalysisBackend::kL2cs:
        return std::make_unique<L2csPipeline>(config, std::move(reference),
                                               std::move(callback));
    case AnalysisBackend::kMefargAu:
        return std::make_unique<MefargAuPipeline>(config, std::move(reference),
                                                   std::move(callback));
    case AnalysisBackend::kNone:
        return std::make_unique<RecognitionPipeline>(config, std::move(reference),
                                                      std::move(callback));
    }
    throw std::invalid_argument("Unknown analysis backend");
}

FaceEmbedding ExtractReferenceEmbedding(const std::string& image_path,
                                         detection::FaceDetector& detector,
                                         recognition::FaceRecognizer& recognizer) {
    cv::Mat ref_img = cv::imread(image_path);
    if (ref_img.empty()) {
        spdlog::error("Failed to load reference image: {}", image_path);
        return {};
    }

    auto boxes = detector.Detect(ref_img);
    if (boxes.empty()) {
        spdlog::error("No face detected in reference image: {}", image_path);
        return {};
    }

    const BoundingBox* best = &boxes[0];
    for (const auto& b : boxes) {
        if (b.confidence > best->confidence) best = &b;
    }

    cv::Mat face_crop = recognition::AlignFace(ref_img, *best);
    spdlog::info("Reference face: confidence={:.2f}", best->confidence);
    return recognizer.Extract(face_crop);
}

}  // namespace omniface
