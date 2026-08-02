#include "omniface/core/analysis/mediapipe/mediapipe_analyzer.h"

#include <cstdio>

#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

#include "omniface/core/analysis/mediapipe/mediapipe_visualizer.h"

namespace omniface::analysis::mediapipe {

MediaPipeAnalyzer::MediaPipeAnalyzer(const MediaPipeAnalyzerConfig& config)
    : config_(config) {
    if (config_.enable_pose) {
        if (config_.landmark_model.empty() || config_.canonical_model.empty()) {
            spdlog::warn("MediaPipeAnalyzer: pose enabled but model paths empty, disabling pose");
            config_.enable_pose = false;
        } else {
            MediaPipePoseEstimator::Config pose_cfg;
            pose_cfg.margin = config_.margin;
            pose_cfg.score_threshold = config_.score_threshold;
            pose_cfg.canonical_model_path = config_.canonical_model;
            pose_cfg.intra_threads = config_.analysis_threads;
            pose_cfg.device = config_.device;

            try {
                pose_estimator_ = std::make_unique<MediaPipePoseEstimator>(
                    config_.landmark_model, pose_cfg);
                spdlog::info("MediaPipeAnalyzer: MediaPipePoseEstimator loaded ({})",
                             config_.landmark_model);
            } catch (const std::exception& e) {
                spdlog::error("MediaPipeAnalyzer: failed to load MediaPipePoseEstimator: {}", e.what());
                config_.enable_pose = false;
            }
        }
    }

}

MediaPipeAnalyzer::~MediaPipeAnalyzer() = default;
MediaPipeAnalyzer::MediaPipeAnalyzer(MediaPipeAnalyzer&&) noexcept = default;
MediaPipeAnalyzer& MediaPipeAnalyzer::operator=(MediaPipeAnalyzer&&) noexcept = default;

MediaPipeAnalysisResult MediaPipeAnalyzer::AnalyzePose(const cv::Mat& frame,
                                              const BoundingBox& bbox,
                                              int track_id) {
    MediaPipeAnalysisResult result;
    result.track_id = track_id;

    if (frame.empty()) return result;

    if (pose_estimator_) {
        auto pose_result = pose_estimator_->EstimatePose(frame, bbox);
        result.pose_valid = pose_result.valid;
        result.pose = std::move(pose_result);
    }

    return result;
}

FrameAnalysisResult MediaPipeAnalyzer::Analyze(const cv::Mat& frame,
                                                const BoundingBox& bbox,
                                                int track_id) {
    auto native = AnalyzePose(frame, bbox, track_id);

    FrameAnalysisResult result;
    result.backend = AnalyzerKind::kMediaPipe;
    result.track_id = track_id;
    if (native.pose_valid) {
        result.valid = true;
        result.head_pose = native.pose.pose;
        result.landmarks = native.pose.landmarks;
    }
    return result;
}

std::vector<MediaPipeAnalysisResult> MediaPipeAnalyzer::AnalyzeBatch(
    const cv::Mat& frame, const std::vector<TrackedFace>& faces) {
    std::vector<MediaPipeAnalysisResult> results;
    results.reserve(faces.size());

    for (const auto& face : faces) {
        results.push_back(AnalyzePose(frame, face.box, face.id));
    }

    return results;
}

void MediaPipeAnalyzer::DrawResults(cv::Mat& frame,
                                     const MediaPipeAnalysisResult& result,
                                     bool draw_landmarks,
                                     bool draw_face_wireframe,
                                     bool draw_face_normal) {
    if (!result.pose_valid || !pose_estimator_) return;

    if (draw_landmarks) {
        MediaPipeVisualizer::DrawLandmarks(frame, result.pose.landmarks);
    }
    if (draw_face_wireframe) {
        MediaPipeVisualizer::DrawProjectedModel(
            frame, result.pose, pose_estimator_->canonical_model());
    }
    if (draw_face_normal) {
        MediaPipeVisualizer::DrawFaceNormal(
            frame, result.pose, pose_estimator_->face_size(),
            pose_estimator_->canonical_model()[1]);
    }

    char pose_text[64];
    std::snprintf(pose_text, sizeof(pose_text), "P:%.2f Y:%.2f R:%.2f",
                  result.pose.pose.pitch, result.pose.pose.yaw,
                  result.pose.pose.roll);
    cv::putText(frame, pose_text,
                cv::Point(static_cast<int>(result.pose.nose_tip.x),
                          static_cast<int>(result.pose.nose_tip.y) - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
}

void MediaPipeAnalyzer::EnsureCsvOpen(const std::string& output_dir) {
    if (onnx_csv_initialized_) return;
    std::string csv_path = output_dir + "/onnx_analysis.csv";
    onnx_csv_.open(csv_path);
    if (onnx_csv_.is_open()) {
        onnx_csv_ << "frame,timestamp,pitch,yaw,roll";
        for (int i = 0; i < 468; ++i)
            onnx_csv_ << ",x_" << i << ",y_" << i;
        onnx_csv_ << "\n";
        onnx_csv_initialized_ = true;
        spdlog::info("ONNX CSV output: {}", csv_path);
    }
}

void MediaPipeAnalyzer::WriteCsvRow(int frame_index, double timestamp,
                                     const MediaPipeAnalysisResult& result) {
    if (!onnx_csv_.is_open()) return;
    onnx_csv_ << frame_index << "," << timestamp << ","
              << result.pose.pose.pitch << ","
              << result.pose.pose.yaw << ","
              << result.pose.pose.roll;
    for (const auto& lm : result.pose.landmarks)
        onnx_csv_ << "," << lm.x << "," << lm.y;
    onnx_csv_ << "\n";
}

void MediaPipeAnalyzer::CloseCsv() {
    if (onnx_csv_.is_open()) onnx_csv_.close();
    onnx_csv_initialized_ = false;
}

}  // namespace omniface::analysis::mediapipe
