#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>

#include "omniface/core/detail/face_aligner.h"
#include "omniface/core/recognition/align.h"
#include "omniface/pipeline/pipeline.h"
#include "omniface/viz/au_visualizer.h"
#include "omniface/viz/gaze_visualizer.h"
#include "omniface/viz/pose_visualizer.h"

namespace omniface::pipeline {

cv::Mat Pipeline::AnalyzeWithOpenFace(const cv::Mat& frame, double timestamp) {
    {
        double nx = (last_target_bbox_.x + last_target_bbox_.width * 0.5) / frame_width_;
        double ny = (last_target_bbox_.y + last_target_bbox_.height * 0.5) / frame_height_;
        openface_->SetFacePreference(nx, ny);
    }

    // 目标切换或首次分析时用 bbox 引导初始化，后续帧利用时序连续跟踪
    if (openface_needs_reinit_ || target_track_id_ != last_openface_track_id_) {
        openface_->AnalyzeVideoFrame(frame, timestamp, last_target_bbox_);
        openface_needs_reinit_ = false;
        last_openface_track_id_ = target_track_id_;
    } else {
        openface_->AnalyzeVideoFrame(frame, timestamp);
    }
    openface_frame_count_++;
    return openface_->GetVisualization();
}

void Pipeline::AnalyzeWithOnnx(cv::Mat& frame) {
    BoundingBox target_box{last_target_bbox_.x,
                           last_target_bbox_.y,
                           last_target_bbox_.x + last_target_bbox_.width,
                           last_target_bbox_.y + last_target_bbox_.height,
                           1.0f,
                           {}};
    cv::Point label_pos(static_cast<int>(target_box.x1), static_cast<int>(target_box.y2));
    int label_y = label_pos.y + 15;

    // 头部姿态
    analysis::PoseEstimationResult pose_res;
    if (pose_estimator_) {
        pose_res = pose_estimator_->EstimatePose(frame, target_box);
        if (pose_res.valid) {
            last_metrics_.pose_valid = true;
            last_metrics_.pitch = pose_res.pose.pitch;
            last_metrics_.yaw = pose_res.pose.yaw;
            last_metrics_.roll = pose_res.pose.roll;

            std::string text = cv::format("P:%.0f Y:%.0f R:%.0f", pose_res.pose.pitch,
                                          pose_res.pose.yaw, pose_res.pose.roll);
            cv::putText(frame, text, cv::Point(label_pos.x, label_y), cv::FONT_HERSHEY_SIMPLEX, 0.6,
                        cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
            label_y += 20;
            viz::PoseVisualizer::DrawLandmarks(frame, pose_res.landmarks);
            viz::PoseVisualizer::DrawProjectedModel(frame, pose_res,
                                                    pose_estimator_->canonical_model());
            viz::PoseVisualizer::DrawFaceNormal(frame, pose_res, pose_estimator_->face_size(),
                                                nose_canonical_);
        }
    }

    bool can_align = pose_res.valid && cfg_.onnx.align_faces && pose_res.landmarks.size() >= 468;

    // 视线估计：优先使用 FaceMesh 关键点对齐，回退到 bbox 裁剪
    if (gaze_estimator_) {
        analysis::GazeResult gaze_res;

        if (can_align) {
            cv::Mat affine =
                detail::AlignFaceSimilarity(pose_res.landmarks, 448, 0.30f, 0.40f, 0.70f, 0.40f);
            if (!affine.empty()) {
                cv::Mat aligned_face = detail::ApplyAlignment(frame, affine, 448);
                gaze_res = gaze_estimator_->EstimateFromAligned(aligned_face);
            }
        }

        if (!gaze_res.valid) {
            gaze_res = gaze_estimator_->Estimate(frame, target_box);
        }

        if (gaze_res.valid) {
            float raw_yaw = gaze_res.yaw;
            float raw_pitch = gaze_res.pitch;

            // 中值滤波去抖动
            constexpr int kMedianWindow = 3;
            gaze_yaw_history_.push_back(raw_yaw);
            gaze_pitch_history_.push_back(raw_pitch);
            while (static_cast<int>(gaze_yaw_history_.size()) > kMedianWindow)
                gaze_yaw_history_.pop_front();
            while (static_cast<int>(gaze_pitch_history_.size()) > kMedianWindow)
                gaze_pitch_history_.pop_front();

            float median_yaw, median_pitch;
            {
                std::vector<float> yaw_buf(gaze_yaw_history_.begin(), gaze_yaw_history_.end());
                std::vector<float> pitch_buf(gaze_pitch_history_.begin(),
                                             gaze_pitch_history_.end());
                std::sort(yaw_buf.begin(), yaw_buf.end());
                std::sort(pitch_buf.begin(), pitch_buf.end());
                median_yaw = yaw_buf[yaw_buf.size() / 2];
                median_pitch = pitch_buf[pitch_buf.size() / 2];
            }

            // EMA 平滑
            float alpha = cfg_.onnx.gaze_smooth_alpha;
            if (alpha > 0.0f && alpha < 1.0f && gaze_smooth_init_) {
                smoothed_gaze_yaw_ = alpha * median_yaw + (1.0f - alpha) * smoothed_gaze_yaw_;
                smoothed_gaze_pitch_ = alpha * median_pitch + (1.0f - alpha) * smoothed_gaze_pitch_;
            } else {
                smoothed_gaze_yaw_ = median_yaw;
                smoothed_gaze_pitch_ = median_pitch;
                gaze_smooth_init_ = true;
            }
            last_metrics_.gaze_valid = true;
            last_metrics_.gaze_yaw = smoothed_gaze_yaw_;
            last_metrics_.gaze_pitch = smoothed_gaze_pitch_;

            std::string text =
                cv::format("Gaze Y:%.2f P:%.2f", smoothed_gaze_yaw_, smoothed_gaze_pitch_);
            cv::putText(frame, text, cv::Point(label_pos.x, label_y), cv::FONT_HERSHEY_SIMPLEX, 0.6,
                        cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
            label_y += 20;
            if (pose_res.valid) {
                viz::GazeVisualizer::DrawGazeVectors(frame, pose_res.landmarks,
                                                     smoothed_gaze_pitch_, smoothed_gaze_yaw_);
            } else {
                viz::GazeVisualizer::DrawGazeArrow(frame, target_box.x1, target_box.y1,
                                                   target_box.x2, target_box.y2,
                                                   smoothed_gaze_pitch_, smoothed_gaze_yaw_);
            }
        }
    }

    // AU 识别
    if (au_analyzer_) {
        analysis::AuAnalysisResult au_res;

        if (can_align) {
            cv::Mat affine =
                detail::AlignFaceSimilarity(pose_res.landmarks, 224, 0.30f, 0.35f, 0.70f, 0.35f);
            if (!affine.empty()) {
                cv::Mat aligned_face = detail::ApplyAlignment(frame, affine, 224);
                au_res = au_analyzer_->AnalyzeFromAligned(aligned_face);
            }
        }

        if (!au_res.valid) {
            au_res = au_analyzer_->Analyze(frame, target_box);
        }

        if (au_res.valid) {
            // 时间窗口平滑
            int temporal_win = cfg_.onnx.au_temporal_window;
            if (temporal_win > 1) {
                std::array<float, 41> raw_cos;
                for (int i = 0; i < 27; ++i) raw_cos[i] = au_res.main_aus[i].cosine_sim;
                for (int i = 0; i < 14; ++i) raw_cos[27 + i] = au_res.sub_aus[i].cosine_sim;

                au_history_.push_back(raw_cos);
                while (static_cast<int>(au_history_.size()) > temporal_win) au_history_.pop_front();

                std::array<float, 41> avg{};
                for (const auto& frame_cos : au_history_)
                    for (int i = 0; i < 41; ++i) avg[i] += frame_cos[i];
                float inv_n = 1.0f / static_cast<float>(au_history_.size());
                for (int i = 0; i < 41; ++i) avg[i] *= inv_n;

                for (int i = 0; i < 27; ++i) {
                    au_res.main_aus[i].cosine_sim = avg[i];
                    au_res.main_aus[i].active = (avg[i] >= cfg_.onnx.au_threshold);
                }
                for (int i = 0; i < 14; ++i) {
                    au_res.sub_aus[i].cosine_sim = avg[27 + i];
                    au_res.sub_aus[i].active = (avg[27 + i] >= cfg_.onnx.au_threshold);
                }
            }

            last_metrics_.aus.reserve(analysis::kMainAuNames.size());
            for (size_t i = 0; i < analysis::kMainAuNames.size(); ++i)
                last_metrics_.aus.emplace_back(analysis::kMainAuNames[i],
                                               au_res.main_aus[i].cosine_sim);
            viz::AuVisualizer::DrawAuText(frame, au_res, cv::Point(label_pos.x, label_y), label_y);
        }
    }
}

// 清理 track_cache_：移除非活跃轨迹的缓存，保留目标轨迹
void Pipeline::PruneTrackCache(const std::vector<TrackedFace>& active_faces) {
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

// 帧处理核心：三阶段流水线
//   Phase 1 — 收集：遍历轨迹，缓存命中直接出结果，否则对齐后加入待识别列表
//   Phase 2 — 批量提取：所有待识别人脸拼成 [N,3,112,112] 一次 ONNX 推理
//   Phase 3 — 匹配：余弦相似度 + 复检逻辑 + 更新缓存
void Pipeline::ProcessFrameCore(const cv::Mat& frame, std::vector<BoundingBox>& detections,
                                std::vector<TrackedFace>& tracked_faces,
                                std::vector<MatchResult>& match_results, int& best_match_idx,
                                float& best_match_sim) {
    match_results.reserve(tracked_faces.size());

    // ===== Phase 1: 收集待识别人脸 =====
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

    bool target_confirmed = false;  // 目标已确认，后续非目标轨迹跳过识别

    for (size_t i = 0; i < tracked_faces.size(); ++i) {
        int tid = tracked_faces[i].id;
        MatchResult result;
        result.tracked_id = tid;
        result.box = tracked_faces[i].box;

        // 动态重检间隔：相似度越高，重检越少; >=0.6 不再重检
        int effective_interval = cfg_.target_recheck_interval;
        if (cfg_.target_recheck_interval > 0 && tid == target_track_id_ &&
            target_track_id_ > 0) {
            auto cache_it = track_cache_.find(tid);
            if (cache_it != track_cache_.end()) {
                float sim = cache_it->second.similarity;
                if (sim >= 0.6f) {
                    effective_interval = 0;   // 确认目标
                } else if (sim >= 0.2f) {
                    effective_interval = 60;
                } else {
                    effective_interval = 30;
                }
            }
        }
        bool recheck_due = effective_interval > 0 && tid == target_track_id_ &&
                           target_track_id_ > 0 &&
                           frame_idx_ - last_target_verify_frame_ >= effective_interval;

        auto cache_it = track_cache_.find(tid);

        // 目标早退：找到目标(sim≥0.3)后，后续轨迹不再识别
        if (cache_it != track_cache_.end() && cache_it->second.is_target &&
            cache_it->second.similarity >= 0.3f) {
            target_confirmed = true;
        }

        // 缓存命中且无需复检 → 直接复用
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
        if (cfg_.min_face_area > 0.0f) {
            float face_w = tracked_faces[i].box.x2 - tracked_faces[i].box.x1;
            float face_h = tracked_faces[i].box.y2 - tracked_faces[i].box.y1;
            float face_area_ratio = (face_w * face_h) / (frame.cols * frame.rows);
            if (face_area_ratio < cfg_.min_face_area) {
                result.valid = false;
                match_results.push_back(result);
                track_cache_[tid] = {false, 0.0f};
                continue;
            }
        }

        // IoU 匹配：从检测框中找回关键点用于对齐
        BoundingBox* matched_det = nullptr;
        {
            float best_iou = cfg_.recognition_iou;
            for (auto& det : detections) {
                float iou = ComputeIoU(tracked_faces[i].box, det);
                if (iou > best_iou) {
                    best_iou = iou;
                    matched_det = &det;
                }
            }
        }

        bool recheck_with_cache = recheck_due && cache_it != track_cache_.end();

        // 复检时对齐/提取失败，回退到缓存值
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
        float min_confidence = cfg_.recognition_confidence_threshold > 0.0f
                                   ? cfg_.recognition_confidence_threshold
                                   : cfg_.detection_confidence;
        if (!quality_fail && matched_det->confidence < min_confidence) quality_fail = true;

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

        // 加入待批量提取列表
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

    // ===== Phase 2: 批量特征提取 =====
    if (!pending.empty()) {
        if (cfg_.async_recognition) {
            // 异步模式：启动后台线程提取，当前帧先用缓存出结果
            for (auto& p : pending) {
                if (p.has_cache) {
                    MatchResult result;
                    result.tracked_id = p.track_id;
                    result.box = tracked_faces[p.tracked_idx].box;
                    result.similarity = p.cached_info.similarity;
                    result.is_match = p.cached_info.is_target;
                    result.valid = true;
                    match_results.push_back(result);
                    if (result.is_match && result.similarity > best_match_sim) {
                        best_match_sim = result.similarity;
                        best_match_idx = static_cast<int>(p.tracked_idx);
                    }
                } else {
                    MatchResult result;
                    result.tracked_id = p.track_id;
                    result.box = tracked_faces[p.tracked_idx].box;
                    result.valid = false;
                    match_results.push_back(result);
                }
            }

            std::vector<cv::Mat> aligned_batch;
            aligned_batch.reserve(pending.size());
            for (auto& p : pending) aligned_batch.push_back(std::move(p.aligned));

            // 捕获元数据，异步执行 ExtractBatch
            std::vector<AsyncRecogItem> async_items;
            async_items.reserve(pending.size());
            for (auto& p : pending) {
                AsyncRecogItem item;
                item.track_id = p.track_id;
                item.box = p.box;
                item.recheck_due = p.recheck_due;
                item.has_cache = p.has_cache;
                if (p.has_cache) item.cached_info = p.cached_info;
                async_items.push_back(std::move(item));
            }

            pending_recognition_ = std::async(std::launch::async,
                [this, aligned_batch = std::move(aligned_batch),
                 async_items = std::move(async_items)]() mutable {
                    auto embeddings = recognizer_->ExtractBatch(aligned_batch);
                    for (size_t j = 0; j < async_items.size() && j < embeddings.size(); ++j) {
                        async_items[j].embedding = std::move(embeddings[j]);
                    }
                    return async_items;
                });
        } else {
            // 同步模式：批量提取 + 立即匹配
            std::vector<cv::Mat> aligned_batch;
            aligned_batch.reserve(pending.size());
            for (auto& p : pending) aligned_batch.push_back(p.aligned);

            auto embeddings = recognizer_->ExtractBatch(aligned_batch);

            // ===== Phase 3: 匹配 & 更新缓存 =====
            for (size_t j = 0; j < pending.size(); ++j) {
                auto& p = pending[j];
                auto& emb = embeddings[j];
                size_t i = p.tracked_idx;
                int tid = p.track_id;

                MatchResult result;
                result.tracked_id = tid;
                result.box = tracked_faces[i].box;

                bool recheck_with_cache = p.recheck_due && p.has_cache;

                auto keep_cached_on_recheck = [&]() {
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
                        keep_cached_on_recheck();
                        continue;
                    }
                    result.valid = false;
                    match_results.push_back(result);
                    track_cache_[tid] = {false, 0.0f};
                    continue;
                }

                // 余弦相似度匹配
                result = matcher_->Match(emb, tracked_faces[i].box, cfg_.match_threshold);
                result.tracked_id = tid;

                // 复检逻辑：匹配失败累计2次则降级目标
                if (p.recheck_due) {
                    last_target_verify_frame_ = frame_idx_;
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

// 更新目标状态：最佳匹配 → 锁定目标; 连续失败 → 降级; 目标消失 → 丢失
bool Pipeline::UpdateTargetState(const std::vector<TrackedFace>& tracked_faces,
                                 const std::vector<MatchResult>& match_results, int best_match_idx,
                                 float best_match_sim) {
    if (best_match_idx >= 0) {
        int new_id = match_results[best_match_idx].tracked_id;
        const auto& box = match_results[best_match_idx].box;
        if (new_id != target_track_id_) {
            target_track_id_ = new_id;
            openface_needs_reinit_ = true;
            last_target_verify_frame_ = frame_idx_;
            target_verify_fails_ = 0;
            target_demote_ = false;
        }
        last_target_bbox_ = cv::Rect_<float>(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        return true;
    }

    if (target_track_id_ > 0) {
        if (target_demote_) {
            target_demote_ = false;
            target_verify_fails_ = 0;
            target_track_id_ = -1;
            openface_needs_reinit_ = true;
            return false;
        }
        for (const auto& tf : tracked_faces) {
            if (tf.id == target_track_id_) {
                last_target_bbox_ = cv::Rect_<float>(tf.box.x1, tf.box.y1, tf.box.x2 - tf.box.x1,
                                                     tf.box.y2 - tf.box.y1);
                return true;
            }
        }
        target_track_id_ = -1;
        openface_needs_reinit_ = true;
    }
    return false;
}

}  // namespace omniface::pipeline
