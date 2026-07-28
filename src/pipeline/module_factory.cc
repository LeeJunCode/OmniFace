#include "omniface/pipeline/module_factory.h"

#include <filesystem>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

#include "omniface/core/recognition/align.h"

namespace omniface {

namespace {

cv::Mat AlignFaceFromBbox(const cv::Mat& frame, const BoundingBox& bbox,
                           int target_size = 112) {
    return recognition::AlignFace(frame, bbox, target_size);
}

}  // namespace

PipelineModules CreateModules(const PipelineConfig& config) {
    PipelineModules mods;

    detection::FaceDetector::Config det_cfg;
    det_cfg.intra_threads = config.model.detection_threads;
    det_cfg.input_width = config.model.detection_input_width;
    det_cfg.input_height = config.model.detection_input_height;
    det_cfg.confidence_threshold = config.thresholds.detection_confidence;
    det_cfg.nms_threshold = config.thresholds.detection_nms;
    mods.detector = std::make_unique<detection::FaceDetector>(
        config.model.detector_model, det_cfg);

    tracking::FaceTracker::Config track_cfg;
    track_cfg.high_threshold = config.thresholds.tracking_high;
    track_cfg.match_threshold = config.thresholds.tracking_match;
    track_cfg.match_threshold_second = config.thresholds.tracking_match_second;
    track_cfg.new_track_threshold = config.thresholds.tracking_new;
    mods.tracker = std::make_unique<tracking::FaceTracker>(track_cfg);
    mods.tracker->Reset();

    if (std::filesystem::is_regular_file(config.model.recognizer_model)) {
        spdlog::info("Loading recognition model: {}", config.model.recognizer_model);
        recognition::FaceRecognizer::Config rec_cfg;
        rec_cfg.intra_threads = config.model.recognition_threads;
        mods.recognizer = std::make_unique<recognition::FaceRecognizer>(
            config.model.recognizer_model, rec_cfg);
        mods.matcher = std::make_unique<recognition::FaceMatcher>();
    }

    if (!config.model.reference_image.empty()) {
        spdlog::info("Loading reference image: {}", config.model.reference_image);
        auto ref_emb = ExtractReferenceEmbedding(config.model.reference_image,
                                                  *mods.detector, *mods.recognizer);
        if (ref_emb.valid) {
            mods.matcher->SetReference(std::move(ref_emb));
            spdlog::info("Reference embedding cached");
        } else {
            spdlog::warn("Failed to extract reference embedding on init");
        }
    }

    if (!config.model.openface_model_dir.empty()
        && !config.behavior.skip_analysis) {
        analysis::OpenFaceProfile profile = analysis::ParseProfile(config.openface.profile);
        analysis::OpenFaceParams of_params = analysis::MakeProfileParams(
            profile, config.model.openface_model_dir);
        analysis::ApplyOverrides(of_params, config.openface,
                                 config.model.openface_model_dir);

        analysis::FaceAnalyzer::Config fa_cfg;
        fa_cfg.model_dir = config.model.openface_model_dir;
        fa_cfg.openface  = std::move(of_params);
        mods.face_analyzer = std::make_unique<analysis::FaceAnalyzer>(fa_cfg);
    }

    return mods;
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

    cv::Mat face_crop = AlignFaceFromBbox(ref_img, *best);
    spdlog::info("Reference face: confidence={:.2f}", best->confidence);
    return recognizer.Extract(face_crop);
}

}  // namespace omniface
