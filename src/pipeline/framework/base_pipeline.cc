#include "omniface/pipeline/framework/base_pipeline.h"

#include <algorithm>
#include <chrono>

#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

namespace omniface {

void BasePipeline::DrawFaceResults(cv::Mat& image,
                                   const std::vector<TrackedFace>& tracked_faces,
                                   const std::vector<MatchResult>& matches,
                                   int target_track_id) {
    for (size_t i = 0; i < tracked_faces.size(); ++i) {
        const auto& tf = tracked_faces[i];
        cv::Point tl(static_cast<int>(tf.box.x1), static_cast<int>(tf.box.y1));
        cv::Point br(static_cast<int>(tf.box.x2), static_cast<int>(tf.box.y2));

        bool is_target = (tf.id == target_track_id && target_track_id > 0);
        cv::Scalar color = is_target ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0);

        cv::rectangle(image, tl, br, color, 2);

        char id_text[32];
        std::snprintf(id_text, sizeof(id_text), "ID:%d", tf.id);
        cv::putText(image, id_text, cv::Point(tl.x, tl.y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1, cv::LINE_AA);

        if (i < matches.size() && matches[i].valid) {
            cv::Scalar sim_color = is_target ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0);
            char sim_text[48];
            std::snprintf(sim_text, sizeof(sim_text), "%s sim:%.2f",
                          is_target ? "TARGET" : "unknown",
                          matches[i].similarity);
            cv::putText(image, sim_text, cv::Point(tl.x, br.y + 15),
                        cv::FONT_HERSHEY_SIMPLEX, 0.45, sim_color, 1, cv::LINE_AA);
        }
    }
}

BasePipeline::BasePipeline(const PipelineConfig& config, FaceEmbedding reference,
                           FrameCallback callback)
    : config_(config)
    , callback_(std::move(callback))
    , modules_(CreateModules(config))
    , output_mgr_(config)
    , target_tracker_(config) {
    if (reference.valid && modules_.matcher) {
        modules_.matcher->SetReference(std::move(reference));
        spdlog::info("Target face enrolled from embedding");
    }
}

BasePipeline::~BasePipeline() = default;

BasePipeline::BasePipeline(BasePipeline&&) noexcept = default;
BasePipeline& BasePipeline::operator=(BasePipeline&&) noexcept = default;

void BasePipeline::EnrollReference(const FaceEmbedding& embedding) {
    if (modules_.matcher && embedding.valid) {
        modules_.matcher->SetReference(embedding);
        spdlog::info("Target face enrolled from cached embedding");
    } else {
        spdlog::warn("EnrollReference failed: no matcher or invalid embedding");
    }
}

void BasePipeline::UpdateThresholds(const ThresholdConfig& thresholds) {
    config_.thresholds = thresholds;
    modules_.detector = std::make_unique<detection::FaceDetector>(
        config_.model.detector_model, MakeDetectorConfig(config_));

    tracking::FaceTracker::Config track_cfg;
    track_cfg.high_threshold = thresholds.tracking_high;
    track_cfg.match_threshold = thresholds.tracking_match;
    track_cfg.match_threshold_second = thresholds.tracking_match_second;
    track_cfg.new_track_threshold = thresholds.tracking_new;
    modules_.tracker = std::make_unique<tracking::FaceTracker>(track_cfg);
    modules_.tracker->Reset();
}

void BasePipeline::SetVideoSource(const std::string& video_path) {
    output_mgr_.SetVideoSource(video_path);
}

void BasePipeline::EnsureOutputInitialized(const cv::Size& frame_size) {
    if (output_initialized_) return;
    output_mgr_.Initialize(frame_size, video_fps_);
    output_initialized_ = true;
}

FrameResult BasePipeline::ProcessFrame(cv::Mat& frame, int frame_index) {
    target_tracker_.SetFrameIndex(frame_index);
    FrameResult result;
    result.frame_index = frame_index;

    EnsureOutputInitialized(frame.size());

    auto t_start = std::chrono::steady_clock::now();

    float ds_scale = 1.0f;
    cv::Mat detect_frame;
    if (config_.behavior.max_frame_width > 0 &&
        frame.cols > config_.behavior.max_frame_width) {
        ds_scale = static_cast<float>(config_.behavior.max_frame_width) / frame.cols;
        cv::resize(frame, detect_frame, cv::Size(), ds_scale, ds_scale, cv::INTER_LINEAR);
    } else {
        detect_frame = frame;
    }

    auto detections = modules_.detector->Detect(detect_frame);

    if (ds_scale < 1.0f) {
        float inv_scale = 1.0f / ds_scale;
        for (auto& d : detections) {
            d.x1 *= inv_scale; d.y1 *= inv_scale;
            d.x2 *= inv_scale; d.y2 *= inv_scale;
            for (auto& k : d.keypoints) k *= inv_scale;
        }
    }

    auto t_detect_done = std::chrono::steady_clock::now();

    auto tracked_faces = modules_.tracker->Update(detections);

    auto t_track_done = std::chrono::steady_clock::now();

    std::vector<MatchResult> matches;
    int best_match_idx = -1;
    float best_match_sim = config_.thresholds.match_threshold;
    int recognition_calls = 0;
    if (modules_.matcher && modules_.recognizer && !tracked_faces.empty()) {
        target_tracker_.ProcessMatches(frame, detections, tracked_faces,
                                       modules_.recognizer.get(),
                                       modules_.matcher.get(),
                                       matches, best_match_idx, best_match_sim,
                                       recognition_calls);
    }

    target_changed_ = target_tracker_.UpdateTargetState(
        tracked_faces, matches, best_match_idx, best_match_sim);

    auto t_recognition_done = std::chrono::steady_clock::now();

    PerformAnalysis(frame, result);

    auto t_analysis_done = std::chrono::steady_clock::now();

    DrawAnalysisOverlays(frame, result);
    DrawFaceResults(frame, tracked_faces, matches, target_tracker_.GetTargetTrackId());

    output_mgr_.WriteFrame(frame);

    auto t_end = std::chrono::steady_clock::now();
    result.total_latency_ms =
        std::chrono::duration<double, std::milli>(t_end - t_start).count();
    result.recognition_latency_ms =
        std::chrono::duration<double, std::milli>(t_recognition_done - t_start).count();
    result.analysis_latency_ms =
        std::chrono::duration<double, std::milli>(t_analysis_done - t_recognition_done).count();
    result.detect_latency_ms =
        std::chrono::duration<double, std::milli>(t_detect_done - t_start).count();
    result.track_latency_ms =
        std::chrono::duration<double, std::milli>(t_track_done - t_detect_done).count();
    result.match_latency_ms =
        std::chrono::duration<double, std::milli>(t_recognition_done - t_track_done).count();
    result.recognition_calls = recognition_calls;
    result.face_count = static_cast<int>(tracked_faces.size());
    result.tracked_faces = std::move(tracked_faces);
    result.match_results = std::move(matches);
    result.annotated_frame = frame;

    if (callback_) {
        callback_(result);
    }

    return result;
}

}  // namespace omniface
