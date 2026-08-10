#include "omniface/pipeline/workers/target_tracker.h"

#include <unordered_set>
#include <vector>

#include <opencv2/imgproc.hpp>

#include "omniface/core/recognition/align.h"

namespace omniface {

void TargetTracker::ApplyCachedMatch(const TrackInfo& cached,
                      MatchResult& result,
                      std::vector<MatchResult>& out_matches,
                      float& best_sim,
                      int& best_idx,
                      int tracked_idx) {
    result.similarity = cached.similarity;
    result.is_match = cached.is_target;
    result.valid = true;
    out_matches.push_back(result);
    if (result.is_match && result.similarity > best_sim) {
        best_sim = result.similarity;
        best_idx = tracked_idx;
    }
}

TargetTracker::TargetTracker(const PipelineConfig& config)
    : config_(config) {}

void TargetTracker::ProcessMatches(const cv::Mat& frame,
                                    std::vector<BoundingBox>& detections,
                                    std::vector<TrackedFace>& tracked_faces,
                                    recognition::FaceRecognizer* recognizer,
                                    recognition::FaceMatcher* matcher,
                                    std::vector<MatchResult>& out_matches,
                                    int& out_best_match_idx,
                                    float& out_best_match_sim) {
    out_matches.clear();
    out_matches.reserve(tracked_faces.size());
    out_best_match_idx = -1;
    out_best_match_sim = config_.thresholds.match_threshold;

    if (!matcher || !recognizer || tracked_faces.empty()) return;

    // ===== 第一阶段：收集待处理项 =====
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

        if (cache_it != track_cache_.end() && cache_it->second.is_target &&
            cache_it->second.similarity >= 0.3f) {
            target_confirmed = true;
        }

        if (cache_it != track_cache_.end() && !recheck_due) {
            result.similarity = cache_it->second.similarity;
            result.is_match = cache_it->second.is_target;
            result.valid = true;
            out_matches.push_back(result);
            if (cache_it->second.is_target && result.similarity > out_best_match_sim) {
                out_best_match_sim = result.similarity;
                out_best_match_idx = static_cast<int>(i);
            }
            continue;
        }

        if (target_confirmed && tid != target_track_id_) {
            if (cache_it != track_cache_.end()) {
                result.similarity = cache_it->second.similarity;
                result.is_match = false;
                result.valid = true;
                out_matches.push_back(result);
            } else {
                result.valid = false;
                out_matches.push_back(result);
                track_cache_[tid] = {false, 0.0f};
            }
            continue;
        }

        if (config_.model.min_face_area > 0.0f) {
            float face_w = tracked_faces[i].box.x2 - tracked_faces[i].box.x1;
            float face_h = tracked_faces[i].box.y2 - tracked_faces[i].box.y1;
            float face_area_ratio = (face_w * face_h) / (frame.cols * frame.rows);
            if (face_area_ratio < config_.model.min_face_area) {
                result.valid = false;
                out_matches.push_back(result);
                track_cache_[tid] = {false, 0.0f};
                continue;
            }
        }

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

        if (!matched_det) {
            if (recheck_with_cache) {
                ApplyCachedMatch(cache_it->second, result, out_matches,
                                 out_best_match_sim, out_best_match_idx,
                                 static_cast<int>(i));
                continue;
            }
            result.valid = false;
            out_matches.push_back(result);
            track_cache_[tid] = {false, 0.0f};
            continue;
        }

        // 质量把关：过滤宽高比异常、关键点越界、置信度不足的检测
        bool quality_fail = false;
        float face_w = tracked_faces[i].box.x2 - tracked_faces[i].box.x1;
        float face_h = tracked_faces[i].box.y2 - tracked_faces[i].box.y1;

        float aspect = (face_h > 0.0f) ? face_w / face_h : 0.0f;
        if (aspect < 0.4f || aspect > 2.5f) quality_fail = true;

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

        float min_confidence = config_.model.recognition_confidence_threshold > 0.0f
                                   ? config_.model.recognition_confidence_threshold
                                   : config_.thresholds.detection_confidence;
        if (!quality_fail && matched_det->confidence < min_confidence)
            quality_fail = true;

        if (quality_fail) {
            if (recheck_with_cache) {
                ApplyCachedMatch(cache_it->second, result, out_matches,
                                 out_best_match_sim, out_best_match_idx,
                                 static_cast<int>(i));
                continue;
            }
            result.valid = false;
            out_matches.push_back(result);
            track_cache_[tid] = {false, 0.0f};
            continue;
        }

        cv::Mat aligned = recognition::AlignFace(frame, *matched_det);
        if (aligned.empty()) {
            if (recheck_with_cache) {
                ApplyCachedMatch(cache_it->second, result, out_matches,
                                 out_best_match_sim, out_best_match_idx,
                                 static_cast<int>(i));
                continue;
            }
            result.valid = false;
            out_matches.push_back(result);
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

    // ===== 第二阶段：批量识别 =====
    if (pending.empty()) {
        PruneTrackCache(tracked_faces);
        return;
    }

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

    // 新轨道：同步识别
    if (!pending_new.empty()) {
        std::vector<cv::Mat> new_batch;
        new_batch.reserve(pending_new.size());
        for (auto& p : pending_new)
            new_batch.push_back(std::move(p.aligned));

        auto embeddings = recognizer->ExtractBatch(new_batch);

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
                out_matches.push_back(result);
                track_cache_[tid] = {false, 0.0f};
                continue;
            }

            result = matcher->Match(emb, tracked_faces[i].box,
                                     config_.thresholds.match_threshold);
            result.tracked_id = tid;
            out_matches.push_back(result);
            track_cache_[tid] = {result.is_match, result.similarity};
            if (result.valid && result.similarity > out_best_match_sim) {
                out_best_match_sim = result.similarity;
                out_best_match_idx = static_cast<int>(i);
            }
        }
    }

    // 已缓存轨道：同步识别（含复查处理）
    if (!pending_recheck.empty()) {
        std::vector<cv::Mat> recheck_batch;
        recheck_batch.reserve(pending_recheck.size());
        for (auto& p : pending_recheck)
            recheck_batch.push_back(p.aligned);

        auto embeddings = recognizer->ExtractBatch(recheck_batch);

        for (size_t j = 0; j < pending_recheck.size(); ++j) {
            auto& p = pending_recheck[j];
            auto& emb = embeddings[j];
            size_t i = p.tracked_idx;
            int tid = p.track_id;

            MatchResult result;
            result.tracked_id = tid;
            result.box = tracked_faces[i].box;

            bool recheck_with_cache = p.recheck_due && p.has_cache;

            if (!emb.valid) {
                if (recheck_with_cache) {
                    ApplyCachedMatch(p.cached_info, result, out_matches,
                                     out_best_match_sim, out_best_match_idx,
                                     static_cast<int>(i));
                    continue;
                }
                result.valid = false;
                out_matches.push_back(result);
                track_cache_[tid] = {false, 0.0f};
                continue;
            }

            result = matcher->Match(emb, tracked_faces[i].box,
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
                out_matches.push_back(result);
                if (result.is_match && result.similarity > out_best_match_sim) {
                    out_best_match_sim = result.similarity;
                    out_best_match_idx = static_cast<int>(i);
                }
                continue;
            }

            out_matches.push_back(result);
            track_cache_[tid] = {result.is_match, result.similarity};
            if (result.valid && result.similarity > out_best_match_sim) {
                out_best_match_sim = result.similarity;
                out_best_match_idx = static_cast<int>(i);
            }
        }
    }

    PruneTrackCache(tracked_faces);
}

void TargetTracker::PruneTrackCache(const std::vector<TrackedFace>& active_faces) {
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

bool TargetTracker::UpdateTargetState(const std::vector<TrackedFace>& tracked_faces,
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

}  // namespace omniface
