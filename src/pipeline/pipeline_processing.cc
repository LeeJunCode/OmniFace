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

void Pipeline::ProcessFrameCore(const cv::Mat& frame, std::vector<BoundingBox>& detections,
                                std::vector<TrackedFace>& tracked_faces,
                                std::vector<MatchResult>& match_results, int& best_match_idx,
                                float& best_match_sim) {
    match_results.reserve(tracked_faces.size());

    for (size_t i = 0; i < tracked_faces.size(); ++i) {
        int tid = tracked_faces[i].id;
        MatchResult result;
        result.tracked_id = tid;
        result.box = tracked_faces[i].box;

        bool recheck_due = cfg_.target_recheck_interval > 0 && tid == target_track_id_ &&
                           target_track_id_ > 0 &&
                           frame_idx_ - last_target_verify_frame_ >= cfg_.target_recheck_interval;

        auto cache_it = track_cache_.find(tid);
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
        bool recheck_with_cache = recheck_due && cache_it != track_cache_.end();

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

        FaceEmbedding emb = recognizer_->Extract(aligned);
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

        result = matcher_->Match(emb, tracked_faces[i].box, cfg_.match_threshold);
        result.tracked_id = tid;

        if (recheck_due) {
            last_target_verify_frame_ = frame_idx_;
            if (result.is_match) {
                target_verify_fails_ = 0;
                track_cache_[tid] = {true, result.similarity};
            } else {
                target_verify_fails_++;
                if (target_verify_fails_ >= 2) {
                    target_demote_ = true;
                    track_cache_.erase(tid);
                } else if (cache_it != track_cache_.end()) {
                    result.is_match = true;
                    result.similarity = cache_it->second.similarity;
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

    PruneTrackCache(tracked_faces);
}

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
