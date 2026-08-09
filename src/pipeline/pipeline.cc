#include "omniface/pipeline/pipeline.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <spdlog/spdlog.h>

#include "omniface/pipeline/decode_worker.h"
#include "omniface/pipeline/module_factory.h"
#include "omniface/pipeline/output_manager.h"
#include "omniface/pipeline/target_tracker.h"

#include "omniface/core/analysis/mefarg_au/mefarg_au_estimator.h"

namespace omniface {

namespace {

void DrawFaceResults(cv::Mat& image, const std::vector<TrackedFace>& tracked_faces,
                      const std::vector<MatchResult>& matches, int target_track_id) {
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

}  // namespace

class Pipeline::Impl {
public:
    PipelineConfig config_;
    FrameCallback callback_;

    PipelineModules modules_;
    DecodeWorker decoder_;
    OutputManager output_mgr_;
    TargetTracker target_tracker_;
    std::string video_input_path_;

    std::atomic<bool> stop_requested_{false};

    Impl(const PipelineConfig& cfg, FrameCallback callback)
        : config_(cfg)
        , callback_(std::move(callback))
        , modules_(CreateModules(cfg))
        , output_mgr_(cfg)
        , target_tracker_(cfg) {}

    FrameResult ProcessFrame(cv::Mat& frame, int frame_index) {
        target_tracker_.SetFrameIndex(frame_index);
        FrameResult result;
        result.frame_index = frame_index;

        output_mgr_.Initialize(frame.size(), decoder_.GetFps());

        auto t_start = std::chrono::steady_clock::now();

        // 对宽帧降采样后再检测，减少计算量
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

        auto tracked_faces = modules_.tracker->Update(detections);

        std::vector<MatchResult> matches;
        int best_match_idx = -1;
        float best_match_sim = config_.thresholds.match_threshold;
        if (modules_.matcher && modules_.recognizer && !tracked_faces.empty()) {
            target_tracker_.ProcessMatches(frame, detections, tracked_faces,
                                           modules_.recognizer.get(),
                                           modules_.matcher.get(),
                                           matches, best_match_idx, best_match_sim);
        }

        bool target_changed = target_tracker_.UpdateTargetState(
            tracked_faces, matches, best_match_idx, best_match_sim);

        auto t_recognition_done = std::chrono::steady_clock::now();

        // ONNX 分析 — 仅处理目标人脸（纯识别模式下跳过）
        if (!config_.behavior.skip_analysis && modules_.onnx_analyzer
            && target_tracker_.HasTarget()) {
            cv::Rect_<float> target_bbox_onnx = target_tracker_.GetTargetBbox();
            if (target_bbox_onnx.width > 0 && target_bbox_onnx.height > 0) {
                BoundingBox bbox;
                bbox.x1 = target_bbox_onnx.x;
                bbox.y1 = target_bbox_onnx.y;
                bbox.x2 = target_bbox_onnx.x + target_bbox_onnx.width;
                bbox.y2 = target_bbox_onnx.y + target_bbox_onnx.height;
                bbox.confidence = 1.0f;
                auto r = modules_.onnx_analyzer->AnalyzePose(
                    frame, bbox, target_tracker_.GetTargetTrackId());

                if (r.pose_valid) {
                    modules_.onnx_analyzer->EnsureCsvOpen(output_mgr_.GetOutputDir());
                    double ts = (decoder_.GetFps() > 0.0)
                        ? (frame_index - 1) / decoder_.GetFps() : 0.0;
                    modules_.onnx_analyzer->WriteCsvRow(frame_index, ts, r);
                }

                result.onnx_results.push_back(std::move(r));
                if (!result.onnx_results.empty() && result.onnx_results.back().pose_valid) {
                    analysis::FrameAnalysisResult unified;
                    unified.backend = analysis::AnalyzerKind::kMediaPipe;
                    unified.track_id = target_tracker_.GetTargetTrackId();
                    unified.valid = true;
                    unified.head_pose = result.onnx_results.back().pose.pose;
                    unified.landmarks = result.onnx_results.back().pose.landmarks;
                    result.analysis_results.push_back(unified);
                }
            }
        }

        // L2CS-Net 视线估计（bbox/gaze EMA 平滑在 Estimate 内部完成）
        if (!config_.behavior.skip_analysis && modules_.l2cs_estimator
            && modules_.l2cs_estimator->IsValid()
            && target_tracker_.HasTarget()) {
            int cur_target_id = target_tracker_.GetTargetTrackId();
            cv::Rect_<float> target_bbox_l2cs = target_tracker_.GetTargetBbox();
            if (target_bbox_l2cs.width > 0 && target_bbox_l2cs.height > 0) {
                BoundingBox bbox;
                bbox.x1 = target_bbox_l2cs.x;
                bbox.y1 = target_bbox_l2cs.y;
                bbox.x2 = target_bbox_l2cs.x + target_bbox_l2cs.width;
                bbox.y2 = target_bbox_l2cs.y + target_bbox_l2cs.height;
                bbox.confidence = 1.0f;

                auto l2cs_result = modules_.l2cs_estimator->Estimate(frame, bbox, cur_target_id);

                if (l2cs_result.valid) {
                    modules_.l2cs_estimator->EnsureCsvOpen(output_mgr_.GetOutputDir());
                    double ts = (decoder_.GetFps() > 0.0)
                        ? (frame_index - 1) / decoder_.GetFps() : 0.0;
                    modules_.l2cs_estimator->WriteCsvRow(frame_index, ts, l2cs_result);
                }

                result.l2cs_results.push_back(std::move(l2cs_result));
                if (!result.l2cs_results.empty() && result.l2cs_results.back().valid) {
                    analysis::FrameAnalysisResult unified;
                    unified.backend = analysis::AnalyzerKind::kL2cs;
                    unified.track_id = target_tracker_.GetTargetTrackId();
                    unified.valid = true;
                    unified.gaze_yaw = result.l2cs_results.back().yaw;
                    unified.gaze_pitch = result.l2cs_results.back().pitch;
                    unified.gaze_vector = result.l2cs_results.back().gaze_vector;
                    result.analysis_results.push_back(unified);
                }
            }
        }

        // MEFARG AU 分析
        if (!config_.behavior.skip_analysis && modules_.au_estimator
            && modules_.au_estimator->IsValid()
            && target_tracker_.HasTarget()) {
            cv::Rect_<float> target_bbox_au = target_tracker_.GetTargetBbox();
            if (target_bbox_au.width > 0 && target_bbox_au.height > 0) {
                BoundingBox bbox;
                bbox.x1 = target_bbox_au.x;
                bbox.y1 = target_bbox_au.y;
                bbox.x2 = target_bbox_au.x + target_bbox_au.width;
                bbox.y2 = target_bbox_au.y + target_bbox_au.height;
                bbox.confidence = 1.0f;

                auto au_result = modules_.au_estimator->Estimate(
                    frame, bbox);
                if (au_result.valid) {
                    modules_.au_estimator->EnsureCsvOpen(output_mgr_.GetOutputDir());
                    double ts = (decoder_.GetFps() > 0.0)
                        ? (frame_index - 1) / decoder_.GetFps() : 0.0;
                    modules_.au_estimator->WriteCsvRow(frame_index, ts, au_result);

                    analysis::FrameAnalysisResult unified;
                    unified.backend = analysis::AnalyzerKind::kMefargAu;
                    unified.track_id = target_tracker_.GetTargetTrackId();
                    unified.valid = true;
                    unified.action_units.reserve(analysis::mefarg_au::kNumAuOutputs);
                    for (int i = 0; i < analysis::mefarg_au::kNumAuOutputs; ++i) {
                        unified.action_units.emplace_back(
                            analysis::mefarg_au::kAuNames[i],
                            static_cast<double>(au_result.au_probs[i]));
                    }
                    result.analysis_results.push_back(std::move(unified));

                    if (config_.mefarg_au.draw_au) {
                        analysis::mefarg_au::MefargAuEstimator::DrawAu(
                            frame, target_bbox_au, au_result,
                            config_.mefarg_au.threshold);
                    }
                }
            }
        }

        // OpenFace 分析：仅对已确认的目标人脸执行
        if (modules_.face_analyzer && modules_.face_analyzer->IsValid()
            && target_tracker_.HasTarget()) {
            cv::Rect_<float> target_bbox = target_tracker_.GetTargetBbox();
            if (target_changed) modules_.face_analyzer->Reset();
            if (target_bbox.width > 0 && target_bbox.height > 0) {
                result.analysis = modules_.face_analyzer->Analyze(frame, target_bbox);

                if (result.analysis.valid && !config_.behavior.skip_analysis) {
                    modules_.face_analyzer->EnsureWriterInitialized(
                        video_input_path_,
                        output_mgr_.GetOutputDir(),
                        decoder_.GetFps(),
                        modules_.face_analyzer->CanAlignFace());
                    cv::Mat aligned = modules_.face_analyzer->GetAlignedFace();
                    modules_.face_analyzer->WriteObservation(
                        result.analysis, frame_index, decoder_.GetFps(), aligned);

                    analysis::FrameAnalysisResult unified;
                    unified.backend = analysis::AnalyzerKind::kOpenFace;
                    unified.track_id = target_tracker_.GetTargetTrackId();
                    unified.valid = result.analysis.valid;
                    if (result.analysis.valid) {
                        unified.head_pose = result.analysis.head_pose;
                        unified.gaze_angle_x = result.analysis.gaze_angle_x;
                        unified.gaze_angle_y = result.analysis.gaze_angle_y;
                        unified.landmarks_2d = result.analysis.landmarks_2d;
                        unified.action_units = result.analysis.action_units;
                        unified.quality_score = result.analysis.quality_score;
                        unified.visible_landmarks = result.analysis.visible_landmarks;
                        unified.detection_certainty = result.analysis.detection_certainty;
                        unified.detection_success = result.analysis.detection_success;
                        unified.gaze_0_x = result.analysis.gaze_0_x;
                        unified.gaze_0_y = result.analysis.gaze_0_y;
                        unified.gaze_0_z = result.analysis.gaze_0_z;
                        unified.gaze_1_x = result.analysis.gaze_1_x;
                        unified.gaze_1_y = result.analysis.gaze_1_y;
                        unified.gaze_1_z = result.analysis.gaze_1_z;
                        unified.pose_Tx = result.analysis.pose_Tx;
                        unified.pose_Ty = result.analysis.pose_Ty;
                        unified.pose_Tz = result.analysis.pose_Tz;
                        unified.pdm_rigid = result.analysis.pdm_rigid;
                        unified.pdm_nonrigid = result.analysis.pdm_nonrigid;
                        unified.eye_landmarks_2d = result.analysis.eye_landmarks_2d;
                        unified.eye_landmarks_3d = result.analysis.eye_landmarks_3d;
                        unified.landmarks_3d = result.analysis.landmarks_3d;
                    }
                    result.analysis_results.push_back(unified);
                }
            }
        }

        auto t_analysis_done = std::chrono::steady_clock::now();

        // MediaPipe 可视化绘制
        if (config_.analysis_backend == AnalysisBackend::kMediaPipe
            && config_.onnx.realtime_display
            && modules_.onnx_analyzer && modules_.onnx_analyzer->has_pose()) {
            for (const auto& r : result.onnx_results) {
                modules_.onnx_analyzer->DrawResults(
                    frame, r,
                    config_.onnx.draw_landmarks,
                    config_.onnx.draw_face_wireframe,
                    config_.onnx.draw_face_normal);
            }
        }

        // L2CS 视线可视化绘制
        if (config_.l2cs.draw_gaze && !result.l2cs_results.empty()
            && target_tracker_.HasTarget()) {
            cv::Rect_<float> cur_bbox = target_tracker_.GetTargetBbox();
            for (const auto& gr : result.l2cs_results) {
                analysis::l2cs::L2csEstimator::DrawGaze(frame, cur_bbox, gr);
            }
        }

        DrawFaceResults(frame, tracked_faces, matches, target_tracker_.GetTargetTrackId());

        output_mgr_.WriteFrame(frame);

        auto t_end = std::chrono::steady_clock::now();
        result.total_latency_ms =
            std::chrono::duration<double, std::milli>(t_end - t_start).count();
        result.recognition_latency_ms =
            std::chrono::duration<double, std::milli>(t_recognition_done - t_start).count();
        bool any_analysis = result.analysis.valid || !result.onnx_results.empty()
                          || !result.l2cs_results.empty()
                          || !result.analysis_results.empty();
        result.analysis_latency_ms = any_analysis
            ? std::chrono::duration<double, std::milli>(t_analysis_done - t_recognition_done).count()
            : 0.0;
        result.face_count = static_cast<int>(tracked_faces.size());
        result.tracked_faces = std::move(tracked_faces);
        result.match_results = std::move(matches);
        result.annotated_frame = frame;

        if (callback_) {
            callback_(result);
        }

        return result;
    }

    void Run() {
        if (video_input_path_.empty()) {
            spdlog::error("No video input specified");
            return;
        }

        decoder_.Start(video_input_path_);
        output_mgr_.SetVideoSource(video_input_path_);

        const char* kWindowName = "OmniFace";
        if (!config_.behavior.headless) {
            cv::namedWindow(kWindowName, cv::WINDOW_NORMAL);
        }

        int frame_count = 0;
        auto last_fps_time = std::chrono::steady_clock::now();
        double fps = 0.0;
        int fps_frame_count = 0;

        while (!stop_requested_.load(std::memory_order_relaxed)) {
            cv::Mat frame;
            if (!decoder_.PollFrame(frame)) {
                if (decoder_.IsStopped()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            frame_count++;
            fps_frame_count++;

            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_fps_time).count();
            if (elapsed >= 1.0) {
                fps = fps_frame_count / elapsed;
                fps_frame_count = 0;
                last_fps_time = now;
            }

            auto result = ProcessFrame(frame, frame_count);

            if (!config_.behavior.headless) {
                cv::imshow(kWindowName, result.annotated_frame);
                if (cv::waitKey(1) == 27) break;
            }
        }

        decoder_.Stop();
        if (modules_.face_analyzer)
            modules_.face_analyzer->CloseWriter();
        if (!config_.behavior.headless) {
            cv::destroyAllWindows();
        }
        spdlog::info("Processed {} frames, exiting", frame_count);
    }
};

Pipeline::Pipeline(const std::string& cfg_path, FrameCallback callback)
    : impl_(std::make_unique<Impl>(LoadPipelineConfig(cfg_path), std::move(callback))) {}

Pipeline::Pipeline(const PipelineConfig& cfg, FrameCallback callback)
    : impl_(std::make_unique<Impl>(cfg, std::move(callback))) {}

Pipeline::~Pipeline() = default;
Pipeline::Pipeline(Pipeline&&) noexcept = default;
Pipeline& Pipeline::operator=(Pipeline&&) noexcept = default;

FrameResult Pipeline::ProcessFrame(cv::Mat& bgr_frame, int frame_index) {
    return impl_->ProcessFrame(bgr_frame, frame_index);
}

void Pipeline::Run() { impl_->Run(); }

void Pipeline::Stop() {
    impl_->stop_requested_.store(true, std::memory_order_relaxed);
    impl_->decoder_.Stop();
}

void Pipeline::OpenVideo(const std::string& path) {
    impl_->video_input_path_ = path;
    impl_->output_mgr_.SetVideoSource(path);
    impl_->decoder_.Start(path);
}

bool Pipeline::PollFrame(cv::Mat& frame) { return impl_->decoder_.PollFrame(frame); }

bool Pipeline::IsDecodeFinished() const {
    return impl_->decoder_.IsStopped();
}

double Pipeline::GetFps() const { return impl_->decoder_.GetFps(); }

int Pipeline::GetFrameCount() const { return impl_->decoder_.GetFrameCount(); }

std::string Pipeline::GetOutputDir() const { return impl_->output_mgr_.GetOutputDir(); }

void Pipeline::UpdateThresholds(const ThresholdConfig& thresholds) {
    impl_->config_.thresholds = thresholds;
    detection::FaceDetector::Config det_cfg;
    det_cfg.intra_threads = impl_->config_.model.detection_threads;
    det_cfg.input_width = impl_->config_.model.detection_input_width;
    det_cfg.input_height = impl_->config_.model.detection_input_height;
    det_cfg.confidence_threshold = thresholds.detection_confidence;
    det_cfg.nms_threshold = thresholds.detection_nms;
    impl_->modules_.detector = std::make_unique<detection::FaceDetector>(
        impl_->config_.model.detector_model, det_cfg);
}

const PipelineConfig& Pipeline::Config() const { return impl_->config_; }

void Pipeline::EnrollReference(const FaceEmbedding& embedding) {
    if (impl_->modules_.matcher && embedding.valid) {
        impl_->modules_.matcher->SetReference(embedding);
        spdlog::info("Target face enrolled from cached embedding");
    } else {
        spdlog::warn("EnrollReference failed: no matcher or invalid embedding");
    }
}

bool Pipeline::HasTarget() const {
    return impl_->modules_.matcher != nullptr && impl_->modules_.matcher->HasReference();
}

}  // namespace omniface
