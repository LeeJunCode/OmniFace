#include "omniface/pipeline/framework/module_factory.h"

#include <filesystem>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

namespace omniface {

PipelineModules CreateModules(const PipelineConfig& config) {
    PipelineModules mods;

    if (!std::filesystem::is_regular_file(config.model.detector_model)) {
        throw std::runtime_error("Detector model not found: " + config.model.detector_model);
    }

    auto det_cfg = MakeDetectorConfig(config);
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
        rec_cfg.device = config.model.device;
        mods.recognizer = std::make_unique<recognition::FaceRecognizer>(
            config.model.recognizer_model, rec_cfg);
        mods.matcher = std::make_unique<recognition::FaceMatcher>();
    }

    if (!config.model.openface_model_dir.empty()
        && !config.behavior.skip_analysis
        && config.analysis_backend == AnalysisBackend::kOpenFace) {
        if (!std::filesystem::is_directory(config.model.openface_model_dir)) {
            spdlog::error("OpenFace model directory not found: {}", config.model.openface_model_dir);
        } else {
            analysis::openface::OpenFaceProfile profile = analysis::openface::ParseProfile(config.openface.profile);
            analysis::openface::OpenFaceParams of_params = analysis::openface::MakeProfileParams(
                profile, config.model.openface_model_dir);
            analysis::openface::ApplyOverrides(of_params, config.openface,
                                     config.model.openface_model_dir);

            analysis::openface::OpenFaceAnalyzer::Config fa_cfg;
            fa_cfg.model_dir = config.model.openface_model_dir;
            fa_cfg.openface  = std::move(of_params);
            mods.face_analyzer = std::make_unique<analysis::openface::OpenFaceAnalyzer>(fa_cfg);
        }
    }

    if (config.analysis_backend == AnalysisBackend::kMediaPipe) {
        if (!std::filesystem::is_regular_file(config.onnx.landmark_model)) {
            spdlog::error("MediaPipe landmark model not found: {}", config.onnx.landmark_model);
        } else if (!std::filesystem::is_regular_file(config.onnx.canonical_model)) {
            spdlog::error("MediaPipe canonical model not found: {}", config.onnx.canonical_model);
        } else {
            analysis::mediapipe::MediaPipeAnalyzerConfig onnx_cfg;
            onnx_cfg.enable_pose = config.onnx.enable_pose;
            onnx_cfg.enable_au   = config.onnx.enable_au;
            onnx_cfg.landmark_model = config.onnx.landmark_model;
            onnx_cfg.canonical_model = config.onnx.canonical_model;
            onnx_cfg.au_model   = config.onnx.au_model;
            onnx_cfg.analysis_threads = config.onnx.analysis_threads;
            onnx_cfg.margin           = config.onnx.margin;
            onnx_cfg.score_threshold  = config.onnx.score_threshold;
            onnx_cfg.device           = config.onnx.device;

            mods.onnx_analyzer = std::make_unique<analysis::mediapipe::MediaPipeAnalyzer>(onnx_cfg);
            spdlog::info("ONNX analysis backend activated (pose={})", config.onnx.enable_pose);
        }
    }

    if (config.analysis_backend == AnalysisBackend::kL2cs) {
        if (!std::filesystem::is_regular_file(config.l2cs.model)) {
            spdlog::error("L2CS model not found: {}", config.l2cs.model);
        } else {
            analysis::l2cs::L2csEstimator::Config l2cs_cfg;
            l2cs_cfg.model_path = config.l2cs.model;
            l2cs_cfg.intra_threads = 1;
            l2cs_cfg.device = config.l2cs.device;

            try {
                mods.l2cs_estimator = std::make_unique<analysis::l2cs::L2csEstimator>(l2cs_cfg);
                spdlog::info("L2CS backend activated (model={})", config.l2cs.model);
            } catch (const std::exception& e) {
                spdlog::error("Failed to load L2csEstimator: {}", e.what());
            }
        }
    }

    if (config.analysis_backend == AnalysisBackend::kMefargAu) {
        if (!std::filesystem::is_regular_file(config.mefarg_au.model)) {
            spdlog::error("MEFARG AU model not found: {}", config.mefarg_au.model);
        } else {
            analysis::mefarg_au::MefargAuEstimator::Config au_cfg;
            au_cfg.model = config.mefarg_au.model;
            au_cfg.intra_threads = config.mefarg_au.intra_threads;
            au_cfg.threshold = config.mefarg_au.threshold;
            au_cfg.device = config.mefarg_au.device;

            try {
                mods.au_estimator = std::make_unique<analysis::mefarg_au::MefargAuEstimator>(au_cfg);
                spdlog::info("MEFARG AU backend activated (model={}, threshold={:.2f})",
                             config.mefarg_au.model, config.mefarg_au.threshold);
            } catch (const std::exception& e) {
                spdlog::error("Failed to load MefargAuEstimator: {}", e.what());
            }
        }
    }

    return mods;
}

}  // namespace omniface
