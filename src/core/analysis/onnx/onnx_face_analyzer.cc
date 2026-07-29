#include "omniface/core/analysis/onnx/onnx_face_analyzer.h"

#include <spdlog/spdlog.h>

namespace omniface::analysis::onnx {

OnnxFaceAnalyzer::OnnxFaceAnalyzer(const OnnxAnalyzerConfig& config)
    : config_(config) {
    if (config_.enable_pose) {
        if (config_.landmark_model.empty() || config_.canonical_model.empty()) {
            spdlog::warn("OnnxFaceAnalyzer: pose enabled but model paths empty, disabling pose");
            config_.enable_pose = false;
        } else {
            PoseEstimator::Config pose_cfg;
            pose_cfg.margin = config_.margin;
            pose_cfg.score_threshold = config_.score_threshold;
            pose_cfg.canonical_model_path = config_.canonical_model;
            pose_cfg.intra_threads = config_.analysis_threads;

            try {
                pose_estimator_ = std::make_unique<PoseEstimator>(
                    config_.landmark_model, pose_cfg);
                spdlog::info("OnnxFaceAnalyzer: PoseEstimator loaded ({})",
                             config_.landmark_model);
            } catch (const std::exception& e) {
                spdlog::error("OnnxFaceAnalyzer: failed to load PoseEstimator: {}", e.what());
                config_.enable_pose = false;
            }
        }
    }

}

OnnxFaceAnalyzer::~OnnxFaceAnalyzer() = default;
OnnxFaceAnalyzer::OnnxFaceAnalyzer(OnnxFaceAnalyzer&&) noexcept = default;
OnnxFaceAnalyzer& OnnxFaceAnalyzer::operator=(OnnxFaceAnalyzer&&) noexcept = default;

OnnxAnalysisResult OnnxFaceAnalyzer::Analyze(const cv::Mat& frame,
                                              const BoundingBox& bbox,
                                              int track_id) {
    OnnxAnalysisResult result;
    result.track_id = track_id;

    if (frame.empty()) return result;

    if (pose_estimator_) {
        auto pose_result = pose_estimator_->EstimatePose(frame, bbox);
        result.pose_valid = pose_result.valid;
        result.pose = std::move(pose_result);
    }

    return result;
}

std::vector<OnnxAnalysisResult> OnnxFaceAnalyzer::AnalyzeBatch(
    const cv::Mat& frame, const std::vector<TrackedFace>& faces) {
    std::vector<OnnxAnalysisResult> results;
    results.reserve(faces.size());

    for (const auto& face : faces) {
        results.push_back(Analyze(frame, face.box, face.id));
    }

    return results;
}

}  // namespace omniface::analysis::onnx
