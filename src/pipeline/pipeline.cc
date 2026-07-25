#include "omniface/pipeline/pipeline.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <spdlog/spdlog.h>

#include "omniface/core/analysis/face_analyzer.h"
#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/align.h"
#include "omniface/core/recognition/face_matcher.h"
#include "omniface/core/recognition/face_recognizer.h"
#include "omniface/core/tracking/face_tracker.h"
#include "omniface/pipeline/bounded_queue.h"

namespace omniface {

namespace {

cv::Mat AlignFaceFromBbox(const cv::Mat& frame, const BoundingBox& bbox,
                           int target_size = 112) {
    return recognition::AlignFace(frame, bbox, target_size);
}

FaceEmbedding ExtractReference(const std::string& image_path,
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

void DrawInfo(cv::Mat& image, int frame_count, double fps) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Frame: %d | FPS: %.1f", frame_count, fps);
    cv::putText(image, buf, cv::Point(10, 25), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
}

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

    std::unique_ptr<detection::FaceDetector> detector_;
    std::unique_ptr<tracking::FaceTracker> tracker_;
    std::unique_ptr<recognition::FaceRecognizer> recognizer_;
    std::unique_ptr<recognition::FaceMatcher> matcher_;
    std::unique_ptr<analysis::FaceAnalyzer> face_analyzer_;

    cv::VideoCapture video_cap_;
    std::atomic<bool> stop_requested_{false};

    // Decode thread
    BoundedQueue<cv::Mat> frame_queue_{4};
    std::thread decode_thread_;
    std::atomic<bool> decode_stop_requested_{false};

    struct TrackInfo {
        bool is_target = false;
        float similarity = 0.0f;
    };
    std::unordered_map<int, TrackInfo> track_cache_;

    int target_track_id_ = -1;
    int target_verify_fails_ = 0;
    bool target_demote_ = false;
    int last_target_verify_frame_ = 0;
    cv::Rect_<float> last_target_bbox_;

    int frame_index_ = 0;

    Impl(const PipelineConfig& cfg, FrameCallback callback)
        : config_(cfg), callback_(std::move(callback)) {
        InitializeModules();
    }

    ~Impl() {
        StopDecode();
    }

    void InitializeModules() {
        detection::FaceDetector::Config det_cfg;
        det_cfg.intra_threads = config_.model.detection_threads;
        det_cfg.input_width = config_.model.detection_input_width;
        det_cfg.input_height = config_.model.detection_input_height;
        det_cfg.confidence_threshold = config_.thresholds.detection_confidence;
        det_cfg.nms_threshold = config_.thresholds.detection_nms;
        detector_ = std::make_unique<detection::FaceDetector>(
            config_.model.detector_model, det_cfg);

        tracking::FaceTracker::Config track_cfg;
        track_cfg.high_threshold = config_.thresholds.tracking_high;
        track_cfg.match_threshold = config_.thresholds.tracking_match;
        track_cfg.match_threshold_second = config_.thresholds.tracking_match_second;
        track_cfg.new_track_threshold = config_.thresholds.tracking_new;
        tracker_ = std::make_unique<tracking::FaceTracker>(track_cfg);
        tracker_->Reset();  // 重置跟踪ID计数器等内部状态

        if (std::filesystem::is_regular_file(config_.model.recognizer_model)) {
            spdlog::info("Loading recognition model: {}", config_.model.recognizer_model);
            recognition::FaceRecognizer::Config rec_cfg;
            rec_cfg.intra_threads = config_.model.recognition_threads;
            recognizer_ = std::make_unique<recognition::FaceRecognizer>(
                config_.model.recognizer_model, rec_cfg);
            matcher_ = std::make_unique<recognition::FaceMatcher>();
        }

        if (!config_.model.reference_image.empty()) {
            spdlog::info("Loading reference image: {}", config_.model.reference_image);
            auto ref_emb = ExtractReference(config_.model.reference_image,
                                             *detector_, *recognizer_);
            if (ref_emb.valid) {
                matcher_->SetReference(std::move(ref_emb));
                spdlog::info("Reference embedding cached");
            } else {
                spdlog::warn("Failed to extract reference embedding on init");
            }
        }

        // OpenFace analysis
        if (!config_.model.openface_model_dir.empty()) {
            analysis::FaceAnalyzer::Config fa_cfg;
            fa_cfg.model_dir = config_.model.openface_model_dir;
            face_analyzer_ = std::make_unique<analysis::FaceAnalyzer>(fa_cfg);
        }
    }

    FrameResult ProcessFrame(cv::Mat& frame, int frame_index) {
        frame_index_ = frame_index;
        FrameResult result;
        result.frame_index = frame_index;

        auto t_start = std::chrono::steady_clock::now();

        // 超宽视频降采样后检测，结果坐标映射回原始分辨率
        float ds_scale = 1.0f;
        cv::Mat detect_frame;
        if (config_.behavior.max_frame_width > 0 &&
            frame.cols > config_.behavior.max_frame_width) {
            ds_scale = static_cast<float>(config_.behavior.max_frame_width) / frame.cols;
            cv::resize(frame, detect_frame, cv::Size(), ds_scale, ds_scale, cv::INTER_LINEAR);
        } else {
            detect_frame = frame;
        }

        auto detections = detector_->Detect(detect_frame);

        // 检测结果坐标映射回原始分辨率
        if (ds_scale < 1.0f) {
            float inv_scale = 1.0f / ds_scale;
            for (auto& d : detections) {
                d.x1 *= inv_scale; d.y1 *= inv_scale;
                d.x2 *= inv_scale; d.y2 *= inv_scale;
                for (auto& k : d.keypoints) k *= inv_scale;
            }
        }

        // Track
        auto tracked_faces = tracker_->Update(detections);

        // Recognize
        std::vector<MatchResult> matches;
        int best_match_idx = -1;
        float best_match_sim = config_.thresholds.match_threshold;
        if (matcher_ && recognizer_ && !tracked_faces.empty()) {
            ProcessFrameCore(frame, detections, tracked_faces, matches,
                             best_match_idx, best_match_sim);
        }

        // Update target state
        UpdateTargetState(tracked_faces, matches, best_match_idx, best_match_sim);

        // OpenFace analysis on target face (renders landmarks/pose/gaze/AUs via Visualizer)
        if (face_analyzer_ && target_track_id_ > 0) {
            cv::Rect_<float> target_bbox(
                last_target_bbox_.x, last_target_bbox_.y,
                last_target_bbox_.width, last_target_bbox_.height);
            if (target_bbox.width > 0 && target_bbox.height > 0) {
                result.analysis = face_analyzer_->Analyze(frame, target_bbox);
            }
        }

        // Draw OmniFace overlays on top of OpenFace visualization
        DrawFaceResults(frame, tracked_faces, matches, target_track_id_);
        DrawInfo(frame, frame_index, 0.0);

        auto t_end = std::chrono::steady_clock::now();
        result.total_latency_ms =
            std::chrono::duration<double, std::milli>(t_end - t_start).count();
        result.face_count = static_cast<int>(tracked_faces.size());
        result.tracked_faces = tracked_faces;
        result.match_results = matches;
        result.annotated_frame = frame;

        if (callback_) {
            callback_(result);
        }

        return result;
    }

    void ProcessFrameCore(const cv::Mat& frame,
                          std::vector<BoundingBox>& detections,
                          std::vector<TrackedFace>& tracked_faces,
                          std::vector<MatchResult>& match_results,
                          int& best_match_idx, float& best_match_sim) {
        match_results.reserve(tracked_faces.size());

        // ===== Phase 1: 收集 =====
        struct PendingItem {
            size_t tracked_idx;
            int track_id;
            cv::Mat aligned;
            BoundingBox box;
            bool recheck_due;
            bool has_cache;
            TrackInfo cached_info;
        };
        std::vector<PendingItem> pending;
        pending.reserve(tracked_faces.size());

        bool target_confirmed = false;

        for (size_t i = 0; i < tracked_faces.size(); ++i) {
            int tid = tracked_faces[i].id;
            MatchResult result;
            result.tracked_id = tid;
            result.box = tracked_faces[i].box;

            // 动态重检间隔
            int effective_interval = config_.behavior.target_recheck_interval;
            if (config_.behavior.target_recheck_interval > 0 &&
                tid == target_track_id_ && target_track_id_ > 0) {
                auto cache_it = track_cache_.find(tid);
                if (cache_it != track_cache_.end()) {
                    float sim = cache_it->second.similarity;
                    if (sim >= 0.6f)
                        effective_interval = 0;
                    else if (sim >= 0.2f)
                        effective_interval = 60;
                    else
                        effective_interval = 30;
                }
            }
            bool recheck_due = effective_interval > 0 && tid == target_track_id_ &&
                               target_track_id_ > 0 &&
                               frame_index_ - last_target_verify_frame_ >= effective_interval;

            auto cache_it = track_cache_.find(tid);

            // 目标早退：找到目标(sim≥0.3)后，后续轨迹不再识别
            if (cache_it != track_cache_.end() && cache_it->second.is_target &&
                cache_it->second.similarity >= 0.3f) {
                target_confirmed = true;
            }

            // 缓存命中且无需复检
            if (cache_it != track_cache_.end() && !recheck_due) {
                result.similarity = cache_it->second.similarity;
                result.is_match = cache_it->second.is_target;
                result.valid = true;
                match_results.push_back(result);
                if (cache_it->second.is_target && result.similarity > best_match_sim) {
                    best_match_sim = result.similarity;
                    best_match_idx = static_cast<int>(i);
                }
                continue;
            }

            // 目标已确认，当前轨迹非目标 → 跳过识别
            if (target_confirmed && tid != target_track_id_) {
                if (cache_it != track_cache_.end()) {
                    result.similarity = cache_it->second.similarity;
                    result.is_match = false;
                    result.valid = true;
                    match_results.push_back(result);
                } else {
                    result.valid = false;
                    match_results.push_back(result);
                    track_cache_[tid] = {false, 0.0f};
                }
                continue;
            }

            // 小人脸过滤
            if (config_.model.min_face_area > 0.0f) {
                float face_w = tracked_faces[i].box.x2 - tracked_faces[i].box.x1;
                float face_h = tracked_faces[i].box.y2 - tracked_faces[i].box.y1;
                float face_area_ratio = (face_w * face_h) / (frame.cols * frame.rows);
                if (face_area_ratio < config_.model.min_face_area) {
                    result.valid = false;
                    match_results.push_back(result);
                    track_cache_[tid] = {false, 0.0f};
                    continue;
                }
            }

            // IoU 匹配：从检测结果中找回关键点
            BoundingBox* matched_det = nullptr;
            {
                float best_iou = config_.model.recognition_iou;
                for (auto& det : detections) {
                    float iou = ComputeIoU(tracked_faces[i].box, det);
                    if (iou > best_iou) {
                        best_iou = iou;
                        matched_det = &det;
                    }
                }
            }

            bool recheck_with_cache = recheck_due && cache_it != track_cache_.end();

            auto keep_cached_on_recheck = [&]() {
                result.similarity = cache_it->second.similarity;
                result.is_match = cache_it->second.is_target;
                result.valid = true;
                match_results.push_back(result);
                if (result.is_match && result.similarity > best_match_sim) {
                    best_match_sim = result.similarity;
                    best_match_idx = static_cast<int>(i);
                }
            };

            if (!matched_det) {
                if (recheck_with_cache) {
                    keep_cached_on_recheck();
                    continue;
                }
                result.valid = false;
                match_results.push_back(result);
                track_cache_[tid] = {false, 0.0f};
                continue;
            }

            // 人脸质量门控 — 三重检查
            bool quality_fail = false;
            float face_w = tracked_faces[i].box.x2 - tracked_faces[i].box.x1;
            float face_h = tracked_faces[i].box.y2 - tracked_faces[i].box.y1;

            // ① 宽高比：过滤极端侧脸/半脸
            float aspect = (face_h > 0.0f) ? face_w / face_h : 0.0f;
            if (aspect < 0.4f || aspect > 2.5f) quality_fail = true;

            // ② 关键点有效性：5个关键点必须在检测框内(±5px容忍)
            if (!quality_fail) {
                for (int k = 0; k < 5; ++k) {
                    float kx = matched_det->keypoints[k * 2];
                    float ky = matched_det->keypoints[k * 2 + 1];
                    if (kx < matched_det->x1 - 5 || kx > matched_det->x2 + 5 ||
                        ky < matched_det->y1 - 5 || ky > matched_det->y2 + 5) {
                        quality_fail = true;
                        break;
                    }
                }
            }

            // ③ 检测置信度
            float min_confidence = config_.model.recognition_confidence_threshold > 0.0f
                                       ? config_.model.recognition_confidence_threshold
                                       : config_.thresholds.detection_confidence;
            if (!quality_fail && matched_det->confidence < min_confidence)
                quality_fail = true;

            if (quality_fail) {
                if (recheck_with_cache) {
                    keep_cached_on_recheck();
                    continue;
                }
                result.valid = false;
                match_results.push_back(result);
                track_cache_[tid] = {false, 0.0f};
                continue;
            }

            // 5点仿射对齐
            cv::Mat aligned = recognition::AlignFace(frame, *matched_det);
            if (aligned.empty()) {
                if (recheck_with_cache) {
                    keep_cached_on_recheck();
                    continue;
                }
                result.valid = false;
                match_results.push_back(result);
                track_cache_[tid] = {false, 0.0f};
                continue;
            }

            PendingItem item;
            item.tracked_idx = i;
            item.track_id = tid;
            item.aligned = std::move(aligned);
            item.box = *matched_det;
            item.recheck_due = recheck_due;
            item.has_cache = (cache_it != track_cache_.end());
            if (item.has_cache) item.cached_info = cache_it->second;
            pending.push_back(std::move(item));
        }

        // ===== Phase 2: 新track同步识别 + 缓存track异步复检 =====
        if (!pending.empty()) {
            // 拆分：新track（无缓存）走同步，缓存track（需复检）可用异步
            std::vector<PendingItem> pending_new;
            std::vector<PendingItem> pending_recheck;
            pending_new.reserve(pending.size());
            pending_recheck.reserve(pending.size());
            for (auto& p : pending) {
                if (p.has_cache)
                    pending_recheck.push_back(std::move(p));
                else
                    pending_new.push_back(std::move(p));
            }

            // 新track：始终同步识别，立即得到结果
            if (!pending_new.empty()) {
                std::vector<cv::Mat> new_batch;
                new_batch.reserve(pending_new.size());
                for (auto& p : pending_new)
                    new_batch.push_back(std::move(p.aligned));

                auto embeddings = recognizer_->ExtractBatch(new_batch);

                for (size_t j = 0; j < pending_new.size(); ++j) {
                    auto& p = pending_new[j];
                    auto& emb = embeddings[j];
                    size_t i = p.tracked_idx;
                    int tid = p.track_id;

                    MatchResult result;
                    result.tracked_id = tid;
                    result.box = tracked_faces[i].box;

                    if (!emb.valid) {
                        result.valid = false;
                        match_results.push_back(result);
                        track_cache_[tid] = {false, 0.0f};
                        continue;
                    }

                    result = matcher_->Match(emb, tracked_faces[i].box,
                                              config_.thresholds.match_threshold);
                    result.tracked_id = tid;
                    match_results.push_back(result);
                    track_cache_[tid] = {result.is_match, result.similarity};
                    if (result.valid && result.similarity > best_match_sim) {
                        best_match_sim = result.similarity;
                        best_match_idx = static_cast<int>(i);
                    }
                }
            }

            // 缓存track：同步识别
            if (!pending_recheck.empty()) {
                std::vector<cv::Mat> recheck_batch;
                recheck_batch.reserve(pending_recheck.size());
                for (auto& p : pending_recheck)
                    recheck_batch.push_back(p.aligned);

                auto embeddings = recognizer_->ExtractBatch(recheck_batch);

                for (size_t j = 0; j < pending_recheck.size(); ++j) {
                    auto& p = pending_recheck[j];
                    auto& emb = embeddings[j];
                    size_t i = p.tracked_idx;
                    int tid = p.track_id;

                    MatchResult result;
                    result.tracked_id = tid;
                    result.box = tracked_faces[i].box;

                    bool recheck_with_cache = p.recheck_due && p.has_cache;

                    auto keep_cached = [&]() {
                        result.similarity = p.cached_info.similarity;
                        result.is_match = p.cached_info.is_target;
                        result.valid = true;
                        match_results.push_back(result);
                        if (result.is_match && result.similarity > best_match_sim) {
                            best_match_sim = result.similarity;
                            best_match_idx = static_cast<int>(i);
                        }
                    };

                    if (!emb.valid) {
                        if (recheck_with_cache) {
                            keep_cached();
                            continue;
                        }
                        result.valid = false;
                        match_results.push_back(result);
                        track_cache_[tid] = {false, 0.0f};
                        continue;
                    }

                    result = matcher_->Match(emb, tracked_faces[i].box,
                                              config_.thresholds.match_threshold);
                    result.tracked_id = tid;

                    if (p.recheck_due) {
                        last_target_verify_frame_ = frame_index_;
                        if (result.is_match) {
                            target_verify_fails_ = 0;
                            track_cache_[tid] = {true, result.similarity};
                        } else {
                            target_verify_fails_++;
                            if (target_verify_fails_ >= 2) {
                                target_demote_ = true;
                                track_cache_.erase(tid);
                            } else if (p.has_cache) {
                                result.is_match = true;
                                result.similarity = p.cached_info.similarity;
                            }
                        }
                        match_results.push_back(result);
                        if (result.is_match && result.similarity > best_match_sim) {
                            best_match_sim = result.similarity;
                            best_match_idx = static_cast<int>(i);
                        }
                        continue;
                    }

                    match_results.push_back(result);
                    track_cache_[tid] = {result.is_match, result.similarity};
                    if (result.valid && result.similarity > best_match_sim) {
                        best_match_sim = result.similarity;
                        best_match_idx = static_cast<int>(i);
                    }
                }
            }
        }

        PruneTrackCache(tracked_faces);
    }

    void PruneTrackCache(const std::vector<TrackedFace>& active_faces) {
        std::unordered_set<int> active_ids;
        for (const auto& tf : active_faces) active_ids.insert(tf.id);

        std::unordered_map<int, TrackInfo> new_cache;
        for (const auto& tf : active_faces) {
            auto it = track_cache_.find(tf.id);
            if (it != track_cache_.end()) new_cache[tf.id] = it->second;
        }
        if (target_track_id_ > 0 && new_cache.find(target_track_id_) == new_cache.end()) {
            auto it = track_cache_.find(target_track_id_);
            if (it != track_cache_.end()) new_cache[target_track_id_] = it->second;
        }
        track_cache_ = std::move(new_cache);
    }

    bool UpdateTargetState(const std::vector<TrackedFace>& tracked_faces,
                           const std::vector<MatchResult>& match_results,
                           int best_match_idx, float best_match_sim) {
        if (best_match_idx >= 0) {
            int new_id = match_results[best_match_idx].tracked_id;
            const auto& box = match_results[best_match_idx].box;
            if (new_id != target_track_id_) {
                target_track_id_ = new_id;
                last_target_verify_frame_ = frame_index_;
                target_verify_fails_ = 0;
                target_demote_ = false;
                if (face_analyzer_) face_analyzer_->Reset();
            }
            last_target_bbox_ = cv::Rect_<float>(box.x1, box.y1,
                                                  box.x2 - box.x1, box.y2 - box.y1);
            return true;
        }

        if (target_track_id_ > 0) {
            if (target_demote_) {
                target_demote_ = false;
                target_verify_fails_ = 0;
                target_track_id_ = -1;
                return false;
            }
            for (const auto& tf : tracked_faces) {
                if (tf.id == target_track_id_) {
                    last_target_bbox_ = cv::Rect_<float>(tf.box.x1, tf.box.y1,
                                                          tf.box.x2 - tf.box.x1,
                                                          tf.box.y2 - tf.box.y1);
                    return true;
                }
            }
            target_track_id_ = -1;
        }
        return false;
    }

    void StartDecode(const std::string& video_path) {
        if (!video_cap_.open(video_path)) {
            spdlog::error("Failed to open video: {}", video_path);
            return;
        }
        spdlog::info("Video opened: {}", video_path);

        decode_stop_requested_.store(false, std::memory_order_relaxed);
        frame_queue_.Reset();

        decode_thread_ = std::thread([this]() {
            cv::Mat frame;
            while (!decode_stop_requested_.load(std::memory_order_relaxed)) {
                if (!video_cap_.read(frame)) break;
                if (frame.empty()) break;
                frame_queue_.Push(frame.clone());
            }
            spdlog::info("Decode thread finished");
            frame_queue_.Stop();
        });
    }

    void StopDecode() {
        decode_stop_requested_.store(true, std::memory_order_relaxed);
        frame_queue_.Stop();
        if (decode_thread_.joinable()) {
            decode_thread_.join();
        }
        video_cap_.release();
    }

    bool PollFrame(cv::Mat& frame) {
        return frame_queue_.TryPop(frame);
    }

    double GetFpsImpl() const {
        double fps = video_cap_.get(cv::CAP_PROP_FPS);
        return (fps > 0.0) ? fps : 24.0;
    }

    int GetFrameCountImpl() const {
        return static_cast<int>(video_cap_.get(cv::CAP_PROP_FRAME_COUNT));
    }

    void Run() {
        if (config_.video.input_path.empty()) {
            spdlog::error("No video input specified");
            return;
        }

        StartDecode(config_.video.input_path);

        cv::VideoWriter writer;
        if (!config_.video.output_path.empty()) {
            int fw = static_cast<int>(video_cap_.get(cv::CAP_PROP_FRAME_WIDTH));
            int fh = static_cast<int>(video_cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
            double fps = GetFpsImpl();
            writer.open(config_.video.output_path,
                        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                        fps, cv::Size(fw, fh));
            if (!writer.isOpened()) {
                spdlog::warn("Failed to create output video: {}",
                             config_.video.output_path);
            }
        }

        const char* kWindowName = "OmniFace";
        cv::namedWindow(kWindowName, cv::WINDOW_NORMAL);

        int frame_count = 0;

        auto last_fps_time = std::chrono::steady_clock::now();
        double fps = 0.0;
        int fps_frame_count = 0;

        while (!stop_requested_.load(std::memory_order_relaxed)) {
            cv::Mat frame;
            if (!frame_queue_.TryPop(frame)) {
                if (frame_queue_.IsStopped()) break;
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
            DrawInfo(result.annotated_frame, frame_count, fps);

            cv::imshow(kWindowName, result.annotated_frame);
            if (writer.isOpened()) writer.write(result.annotated_frame);

            if (cv::waitKey(1) == 27) break;
        }

        StopDecode();
        writer.release();
        cv::destroyAllWindows();
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
    impl_->StopDecode();
}

void Pipeline::OpenVideo(const std::string& path) { impl_->StartDecode(path); }

bool Pipeline::PollFrame(cv::Mat& frame) { return impl_->PollFrame(frame); }

bool Pipeline::IsDecodeFinished() const {
    return impl_->frame_queue_.IsStopped();
}

double Pipeline::GetFps() const { return impl_->GetFpsImpl(); }

int Pipeline::GetFrameCount() const { return impl_->GetFrameCountImpl(); }

void Pipeline::UpdateThresholds(const ThresholdConfig& thresholds) {
    impl_->config_.thresholds = thresholds;
    detection::FaceDetector::Config det_cfg;
    det_cfg.intra_threads = impl_->config_.model.detection_threads;
    det_cfg.input_width = impl_->config_.model.detection_input_width;
    det_cfg.input_height = impl_->config_.model.detection_input_height;
    det_cfg.confidence_threshold = thresholds.detection_confidence;
    det_cfg.nms_threshold = thresholds.detection_nms;
    impl_->detector_ = std::make_unique<detection::FaceDetector>(
        impl_->config_.model.detector_model, det_cfg);
}

const PipelineConfig& Pipeline::Config() const { return impl_->config_; }

void Pipeline::EnrollReference(const FaceEmbedding& embedding) {
    if (impl_->matcher_ && embedding.valid) {
        impl_->matcher_->SetReference(embedding);
        spdlog::info("Target face enrolled from cached embedding");
    } else {
        spdlog::warn("EnrollReference failed: no matcher or invalid embedding");
    }
}

bool Pipeline::HasTarget() const {
    return impl_->matcher_ != nullptr && impl_->matcher_->HasReference();
}

}  // namespace omniface
