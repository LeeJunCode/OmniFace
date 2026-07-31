#include "omniface/pipeline/pipeline.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <spdlog/spdlog.h>

#include "omniface/core/analysis/onnx/pose_visualizer.h"
#include "omniface/pipeline/decode_worker.h"
#include "omniface/pipeline/module_factory.h"
#include "omniface/pipeline/openface_writer.h"
#include "omniface/pipeline/output_manager.h"
#include "omniface/pipeline/target_tracker.h"

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
    OpenFaceWriter openface_writer_;
    TargetTracker target_tracker_;

    std::atomic<bool> stop_requested_{false};
    std::ofstream onnx_csv_;
    bool onnx_csv_initialized_ = false;
    std::ofstream gaze_csv_;
    bool gaze_csv_initialized_ = false;

    // 对视线角度和边界框做 EMA 平滑，减少帧间抖动
    float smooth_yaw_ = 0.0f;
    float smooth_pitch_ = 0.0f;
    float smooth_bbox_x_ = 0.0f;
    float smooth_bbox_y_ = 0.0f;
    float smooth_bbox_w_ = 0.0f;
    float smooth_bbox_h_ = 0.0f;
    bool gaze_smooth_init_ = false;
    int prev_target_id_ = -1;

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

        output_mgr_.Initialize(frame.size(), decoder_.GetFps(),
                              modules_.detector.get());

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
                auto r = modules_.onnx_analyzer->Analyze(
                    frame, bbox, target_tracker_.GetTargetTrackId());

                if (r.pose_valid) {
                    if (!onnx_csv_initialized_) {
                        std::string csv_path = output_mgr_.GetOutputDir() +
                                               "/onnx_analysis.csv";
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
                    if (onnx_csv_.is_open()) {
                        double ts = (decoder_.GetFps() > 0.0)
                            ? (frame_index - 1) / decoder_.GetFps() : 0.0;
                        onnx_csv_ << frame_index << "," << ts << ","
                                  << r.pose.pose.pitch << ","
                                  << r.pose.pose.yaw << ","
                                  << r.pose.pose.roll;
                        for (const auto& lm : r.pose.landmarks)
                            onnx_csv_ << "," << lm.x << "," << lm.y;
                        onnx_csv_ << "\n";
                    }
                }

                result.onnx_results.push_back(std::move(r));
            }
        }

        // 视线估计 — 独立于分析后端运行
        if (!config_.behavior.skip_analysis && modules_.gaze_estimator
            && modules_.gaze_estimator->IsValid()
            && target_tracker_.HasTarget()) {
            int cur_target_id = target_tracker_.GetTargetTrackId();
            bool target_id_changed = (cur_target_id != prev_target_id_);
            if (target_id_changed) prev_target_id_ = cur_target_id;
            cv::Rect_<float> target_bbox_gaze = target_tracker_.GetTargetBbox();
            if (target_bbox_gaze.width > 0 && target_bbox_gaze.height > 0) {
                // EMA 平滑边界框，减少裁剪抖动（这是视线抖动的根本原因）
                if (!gaze_smooth_init_ || target_id_changed) {
                    smooth_bbox_x_ = target_bbox_gaze.x;
                    smooth_bbox_y_ = target_bbox_gaze.y;
                    smooth_bbox_w_ = target_bbox_gaze.width;
                    smooth_bbox_h_ = target_bbox_gaze.height;
                } else {
                    constexpr float kBboxAlpha = 0.25f;
                    smooth_bbox_x_ = kBboxAlpha * target_bbox_gaze.x
                                   + (1.0f - kBboxAlpha) * smooth_bbox_x_;
                    smooth_bbox_y_ = kBboxAlpha * target_bbox_gaze.y
                                   + (1.0f - kBboxAlpha) * smooth_bbox_y_;
                    smooth_bbox_w_ = kBboxAlpha * target_bbox_gaze.width
                                   + (1.0f - kBboxAlpha) * smooth_bbox_w_;
                    smooth_bbox_h_ = kBboxAlpha * target_bbox_gaze.height
                                   + (1.0f - kBboxAlpha) * smooth_bbox_h_;
                }
                cv::Rect_<float> smooth_bbox(smooth_bbox_x_, smooth_bbox_y_,
                                             smooth_bbox_w_, smooth_bbox_h_);

                BoundingBox bbox;
                bbox.x1 = smooth_bbox.x;
                bbox.y1 = smooth_bbox.y;
                bbox.x2 = smooth_bbox.x + smooth_bbox.width;
                bbox.y2 = smooth_bbox.y + smooth_bbox.height;
                bbox.confidence = 1.0f;

                auto gaze_result = modules_.gaze_estimator->Estimate(frame, bbox);

                if (gaze_result.valid) {
                    if (!gaze_smooth_init_ || target_id_changed) {
                        smooth_yaw_ = gaze_result.yaw;
                        smooth_pitch_ = gaze_result.pitch;
                        gaze_smooth_init_ = true;
                    } else {
                        constexpr float kGazeAlpha = 0.2f;
                        smooth_yaw_ = kGazeAlpha * gaze_result.yaw
                                    + (1.0f - kGazeAlpha) * smooth_yaw_;
                        smooth_pitch_ = kGazeAlpha * gaze_result.pitch
                                      + (1.0f - kGazeAlpha) * smooth_pitch_;
                    }
                    gaze_result.yaw = smooth_yaw_;
                    gaze_result.pitch = smooth_pitch_;
                    float yaw_r = smooth_yaw_ * static_cast<float>(CV_PI) / 180.0f;
                    float pitch_r = smooth_pitch_ * static_cast<float>(CV_PI) / 180.0f;
                    gaze_result.gaze_vector = cv::Vec3f(
                        -std::cos(pitch_r) * std::sin(yaw_r),
                        -std::sin(pitch_r),
                        -std::cos(pitch_r) * std::cos(yaw_r));
                    if (!gaze_csv_initialized_) {
                        std::string csv_path = output_mgr_.GetOutputDir() +
                                               "/gaze_analysis.csv";
                        gaze_csv_.open(csv_path);
                        if (gaze_csv_.is_open()) {
                            gaze_csv_ << "frame,timestamp,gaze_yaw,gaze_pitch,gaze_x,gaze_y,gaze_z\n";
                            gaze_csv_initialized_ = true;
                            spdlog::info("Gaze CSV output: {}", csv_path);
                        }
                    }
                    if (gaze_csv_.is_open()) {
                        double ts = (decoder_.GetFps() > 0.0)
                            ? (frame_index - 1) / decoder_.GetFps() : 0.0;
                        gaze_csv_ << frame_index << "," << ts << ","
                                  << gaze_result.yaw << "," << gaze_result.pitch << ","
                                  << gaze_result.gaze_vector[0] << ","
                                  << gaze_result.gaze_vector[1] << ","
                                  << gaze_result.gaze_vector[2] << "\n";
                    }
                }

                result.gaze_results.push_back(std::move(gaze_result));
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
                    openface_writer_.Initialize(config_.video.input_path,
                                                output_mgr_.GetOutputDir(),
                                                decoder_.GetFps(),
                                                modules_.face_analyzer->CanAlignFace());
                    cv::Mat aligned = modules_.face_analyzer->GetAlignedFace();
                    openface_writer_.WriteObservation(result.analysis, frame_index,
                                                      decoder_.GetFps(), aligned);
                }
            }
        }

        auto t_analysis_done = std::chrono::steady_clock::now();

        // 在帧上绘制 ONNX 分析结果（关键点、线框、法线、姿态角度）
        if (config_.analysis_backend == AnalysisBackend::kOnnx
            && config_.onnx.realtime_display
            && modules_.onnx_analyzer && modules_.onnx_analyzer->has_pose()) {
            const auto* pose_est = modules_.onnx_analyzer->pose_estimator();
            for (const auto& r : result.onnx_results) {
                if (!r.pose_valid) continue;
                if (config_.onnx.draw_landmarks) {
                    analysis::onnx::PoseVisualizer::DrawLandmarks(frame, r.pose.landmarks);
                }
                if (config_.onnx.draw_face_wireframe) {
                    analysis::onnx::PoseVisualizer::DrawProjectedModel(
                        frame, r.pose, pose_est->canonical_model());
                }
                if (config_.onnx.draw_face_normal) {
                    analysis::onnx::PoseVisualizer::DrawFaceNormal(
                        frame, r.pose, pose_est->face_size(),
                        pose_est->canonical_model()[1]);
                }
                char pose_text[64];
                std::snprintf(pose_text, sizeof(pose_text), "P:%.2f Y:%.2f R:%.2f",
                              r.pose.pose.pitch, r.pose.pose.yaw, r.pose.pose.roll);
                cv::putText(frame, pose_text,
                            cv::Point(static_cast<int>(r.pose.nose_tip.x),
                                      static_cast<int>(r.pose.nose_tip.y) - 10),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5,
                            cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
            }
        }

        // 绘制视线结果（独立于分析后端）
        // 与官方 draw_gaze 一致：箭头长度 = bbox 宽度，
        // dx = -length*sin(yaw)*cos(pitch), dy = -length*sin(pitch)
        // 箭头起点使用当前（未平滑）bbox 实时跟踪人脸移动，方向使用平滑后的角度减少抖动
        if (config_.gaze.draw_gaze && !result.gaze_results.empty()
            && target_tracker_.HasTarget() && gaze_smooth_init_) {
            cv::Rect_<float> cur_bbox = target_tracker_.GetTargetBbox();
            float cx = cur_bbox.x + cur_bbox.width / 2.0f;
            float cy = cur_bbox.y + cur_bbox.height / 2.0f;
            cv::Point2f start(cx, cy);
            float arrow_len = cur_bbox.width;
            for (const auto& gr : result.gaze_results) {
                if (!gr.valid) continue;
                cv::Point2f end(start.x + gr.gaze_vector[0] * arrow_len,
                                start.y + gr.gaze_vector[1] * arrow_len);
                cv::circle(frame, start, 4, cv::Scalar(255, 0, 255), -1, cv::LINE_AA);
                cv::arrowedLine(frame, start, end, cv::Scalar(255, 0, 255), 2,
                                cv::LINE_AA, 0, 0.25);
                char gaze_text[48];
                std::snprintf(gaze_text, sizeof(gaze_text),
                              "gaze:%.1f,%.1f", gr.yaw, gr.pitch);
                cv::putText(frame, gaze_text,
                            cv::Point(static_cast<int>(end.x),
                                      static_cast<int>(end.y) - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.4,
                            cv::Scalar(255, 0, 255), 1, cv::LINE_AA);
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
                          || !result.gaze_results.empty();
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
        if (config_.video.input_path.empty()) {
            spdlog::error("No video input specified");
            return;
        }

        decoder_.Start(config_.video.input_path);

        cv::VideoWriter writer;
        if (!config_.video.output_path.empty()) {
            int fw = static_cast<int>(decoder_.GetFrameCount() > 0 ?
                decoder_.GetFrameCount() : 0);
            double fps = decoder_.GetFps();
            writer.open(config_.video.output_path,
                        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                        fps, cv::Size(fw > 0 ? 0 : 0, 0));
        }

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
            if (writer.isOpened()) writer.write(result.annotated_frame);
        }

        decoder_.Stop();
        writer.release();
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

void Pipeline::OpenVideo(const std::string& path) { impl_->decoder_.Start(path); }

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
