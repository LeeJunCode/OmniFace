#include "omniface/core/tracking/bytetrack_engine.h"

#include <unordered_set>

#include "omniface/core/tracking/kalman_filter.h"
#include "omniface/core/tracking/lapjv.h"
#include "omniface/core/tracking/strack.h"

namespace omniface::tracking {

void JointStracks(STrack::List& dst, const STrack::List& src) {
    std::unordered_set<int> existing;
    for (const auto& t : dst) existing.insert(t.TrackId());
    for (const auto& t : src) {
        if (existing.find(t.TrackId()) == existing.end()) {
            dst.push_back(t);
            existing.insert(t.TrackId());
        }
    }
}

STrack::List SubStracks(const STrack::List& tlista, const STrack::List& tlistb) {
    std::unordered_set<int> ids_b;
    for (const auto& t : tlistb) ids_b.insert(t.TrackId());

    STrack::List result;
    result.reserve(tlista.size());
    for (const auto& t : tlista) {
        if (ids_b.find(t.TrackId()) == ids_b.end()) {
            result.push_back(t);
        }
    }
    return result;
}

void RemoveDuplicateStracks(STrack::List& a, STrack::List& b, float iou_thresh = 0.15f) {
    if (a.empty() || b.empty()) return;

    std::vector<float> dists(static_cast<size_t>(a.size() * b.size()), 1.0f);
    for (size_t i = 0; i < a.size(); i++) {
        for (size_t j = 0; j < b.size(); j++) {
            float iou = a[i].ComputeIoU(b[j]);
            if (iou > 0.0f) dists[i * b.size() + j] = 1.0f - iou;
        }
    }

    std::vector<bool> remove_a(a.size(), false);
    std::vector<bool> remove_b(b.size(), false);

    for (size_t i = 0; i < a.size(); i++) {
        for (size_t j = 0; j < b.size(); j++) {
            if (dists[i * b.size() + j] <= (1.0f - iou_thresh)) {
                if (a[i].EndFrame() > b[j].EndFrame()) {
                    remove_b[j] = true;
                } else {
                    remove_a[i] = true;
                }
            }
        }
    }

    STrack::List new_a, new_b;
    for (size_t i = 0; i < a.size(); i++)
        if (!remove_a[i]) new_a.push_back(std::move(a[i]));
    for (size_t j = 0; j < b.size(); j++)
        if (!remove_b[j]) new_b.push_back(std::move(b[j]));
    a = std::move(new_a);
    b = std::move(new_b);
}

void LinearAssignment(const std::vector<float>& cost_matrix, int d_size, int t_size, float thresh,
                      std::vector<std::pair<int, int>>& matches, std::vector<int>& unmatched_d,
                      std::vector<int>& unmatched_t) {
    matches.clear();
    unmatched_d.clear();
    unmatched_t.clear();

    if (d_size == 0 || t_size == 0) {
        for (int i = 0; i < d_size; i++) unmatched_d.push_back(i);
        for (int i = 0; i < t_size; i++) unmatched_t.push_back(i);
        return;
    }

    std::vector<int> x, y;
    Lapjv(cost_matrix.data(), d_size, t_size, false, thresh, x, y);

    for (int i = 0; i < d_size; i++) {
        if (x[i] >= 0)
            matches.emplace_back(i, x[i]);
        else
            unmatched_d.push_back(i);
    }
    for (int i = 0; i < t_size; i++) {
        if (y[i] < 0) unmatched_t.push_back(i);
    }
}

class ByteTrackEngine::Impl {
public:
    explicit Impl(Config config) : config_(std::move(config)) {
        max_time_lost_ = config_.track_buffer;
    }

    // ByteTrack 核心：高分检测先匹配，剩余低分检测二次匹配，激活/丢失/移除三级轨迹管理
    std::vector<Track> Update(const std::vector<Detection>& detections) {
        frame_id_++;

        // 按置信度分为高/低分两组
        STrack::List det_high, det_low;
        det_high.reserve(detections.size());
        det_low.reserve(detections.size());

        for (const auto& d : detections) {
            if (d.tlwh[2] <= 0.0f || d.tlwh[3] <= 0.0f) continue;
            if (d.score <= 0.0f) continue;

            std::array<float, 4> tlwh = {d.tlwh[0], d.tlwh[1], d.tlwh[2], d.tlwh[3]};
            if (d.score >= config_.track_thresh) {
                det_high.emplace_back(tlwh, d.score);
            } else {
                det_low.emplace_back(tlwh, d.score);
            }
        }

        std::vector<STrack*> track_pool;
        for (auto& t : tracked_stracks_) {
            if (t.State() != STrackState::kRemoved) track_pool.push_back(&t);
        }
        for (auto& t : lost_stracks_) {
            track_pool.push_back(&t);
        }

        STrack::MultiPredict(tracked_stracks_, kalman_filter_);
        STrack::MultiPredict(lost_stracks_, kalman_filter_);

        std::vector<const STrack*> track_ptrs(track_pool.begin(), track_pool.end());
        auto dists = STrack::IoUDistance(track_ptrs, det_high, kalman_filter_);
        int d_size = static_cast<int>(det_high.size());
        int t_size = static_cast<int>(track_pool.size());

        std::vector<std::pair<int, int>> matches;
        std::vector<int> unmatched_t, unmatched_d;
        float cost_limit = 1.0f - config_.match_thresh;
        LinearAssignment(dists, d_size, t_size, cost_limit, matches, unmatched_d, unmatched_t);

        STrack::List activated;
        STrack::List refind;
        for (auto [det_idx, track_idx] : matches) {
            STrack* track = track_pool[track_idx];
            STrack* det = &det_high[det_idx];

            if (track->State() == STrackState::kTracked) {
                track->UpdateTrack(*det, frame_id_, kalman_filter_);
                activated.push_back(*track);
            } else {
                track->ReActivate(*det, frame_id_, kalman_filter_);
                refind.push_back(*track);
            }
        }

        std::vector<STrack*> r_tracked;
        for (int idx : unmatched_t) {
            if (track_pool[idx]->State() == STrackState::kTracked) {
                r_tracked.push_back(track_pool[idx]);
            }
        }

        std::vector<STrack*> to_mark_lost;

        if (!r_tracked.empty() && !det_low.empty()) {
            std::vector<const STrack*> r_ptrs(r_tracked.begin(), r_tracked.end());
            auto dists2 = STrack::IoUDistance(r_ptrs, det_low, kalman_filter_);
            int d2_size = static_cast<int>(det_low.size());
            int t2_size = static_cast<int>(r_tracked.size());

            std::vector<std::pair<int, int>> matches2;
            std::vector<int> unmatched_t2, unmatched_d2;
            float cost_limit2 = 1.0f - config_.match_thresh_second;
            LinearAssignment(dists2, d2_size, t2_size, cost_limit2, matches2, unmatched_t2,
                             unmatched_d2);

            for (auto [det_idx, track_idx] : matches2) {
                STrack* track = r_tracked[track_idx];
                STrack* det = &det_low[det_idx];
                if (track->State() == STrackState::kTracked) {
                    track->UpdateTrack(*det, frame_id_, kalman_filter_);
                    activated.push_back(*track);
                } else {
                    track->ReActivate(*det, frame_id_, kalman_filter_);
                    refind.push_back(*track);
                }
            }

            for (int idx : unmatched_t2) to_mark_lost.push_back(r_tracked[idx]);
        } else {
            for (int idx : unmatched_t) {
                if (track_pool[idx]->State() == STrackState::kTracked) {
                    to_mark_lost.push_back(track_pool[idx]);
                }
            }
        }

        for (auto* t : to_mark_lost) t->MarkLost();

        STrack::List unmatched_high_after;
        unmatched_high_after.reserve(unmatched_d.size());
        for (int idx : unmatched_d) unmatched_high_after.push_back(det_high[idx]);

        std::vector<STrack*> unconfirmed_now;
        for (auto& t : tracked_stracks_) {
            if (!t.IsActivated() && t.State() != STrackState::kRemoved) {
                unconfirmed_now.push_back(&t);
            }
        }

        if (!unconfirmed_now.empty() && !unmatched_high_after.empty()) {
            std::vector<const STrack*> u_ptrs(unconfirmed_now.begin(), unconfirmed_now.end());
            auto dists3 = STrack::IoUDistance(u_ptrs, unmatched_high_after, kalman_filter_);
            int d3_size = static_cast<int>(unmatched_high_after.size());
            int t3_size = static_cast<int>(unconfirmed_now.size());

            std::vector<std::pair<int, int>> matches3;
            std::vector<int> unmatched_t3, unmatched_d3;
            LinearAssignment(dists3, d3_size, t3_size, 0.3f, matches3, unmatched_d3, unmatched_t3);

            for (auto [det_idx, track_idx] : matches3) {
                unconfirmed_now[track_idx]->UpdateTrack(unmatched_high_after[det_idx], frame_id_,
                                                        kalman_filter_);
                activated.push_back(*unconfirmed_now[track_idx]);
            }

            for (int idx : unmatched_t3) unconfirmed_now[idx]->MarkRemoved();

            STrack::List still_unmatched;
            still_unmatched.reserve(unmatched_d3.size());
            for (int idx : unmatched_d3) still_unmatched.push_back(unmatched_high_after[idx]);
            unmatched_high_after = std::move(still_unmatched);
        } else {
            for (auto* t : unconfirmed_now) t->MarkRemoved();
        }

        for (auto& det : unmatched_high_after) {
            if (det.Score() >= config_.high_thresh) {
                det.Activate(kalman_filter_, frame_id_);
                activated.push_back(std::move(det));
            }
        }

        for (auto& t : lost_stracks_) {
            if (frame_id_ - t.EndFrame() > max_time_lost_) {
                t.MarkRemoved();
            }
        }

        {
            STrack::List alive_tracked;
            STrack::List newly_lost;
            for (auto& t : tracked_stracks_) {
                if (t.State() == STrackState::kTracked) {
                    alive_tracked.push_back(std::move(t));
                } else if (t.State() == STrackState::kLost) {
                    newly_lost.push_back(std::move(t));
                }
            }
            tracked_stracks_ = std::move(alive_tracked);

            JointStracks(tracked_stracks_, activated);
            JointStracks(tracked_stracks_, refind);

            lost_stracks_ = SubStracks(lost_stracks_, tracked_stracks_);

            for (auto& t : newly_lost) lost_stracks_.push_back(std::move(t));
        }

        {
            STrack::List alive_lost;
            for (auto& t : lost_stracks_) {
                if (t.State() != STrackState::kRemoved) alive_lost.push_back(std::move(t));
            }
            lost_stracks_ = std::move(alive_lost);
        }

        RemoveDuplicateStracks(tracked_stracks_, lost_stracks_);

        std::vector<Track> output;
        output.reserve(tracked_stracks_.size());
        for (const auto& t : tracked_stracks_) {
            if (t.IsActivated()) {
                output.push_back({t.TrackId(),
                                  {t.Tlwh()[0], t.Tlwh()[1], t.Tlwh()[2], t.Tlwh()[3]},
                                  t.Score(),
                                  t.IsActivated(),
                                  t.HitStreak(),
                                  t.MissStreak()});
            }
        }
        return output;
    }

    void Reset() {
        tracked_stracks_.clear();
        lost_stracks_.clear();
        frame_id_ = 0;
    }

private:
    Config config_;
    int frame_id_ = 0;
    int max_time_lost_;

    KalmanFilter kalman_filter_;

    STrack::List tracked_stracks_;
    STrack::List lost_stracks_;
};

ByteTrackEngine::ByteTrackEngine() : impl_(std::make_unique<Impl>(Config{})) {}
ByteTrackEngine::ByteTrackEngine(Config config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

ByteTrackEngine::~ByteTrackEngine() = default;

ByteTrackEngine::ByteTrackEngine(ByteTrackEngine&&) noexcept = default;
ByteTrackEngine& ByteTrackEngine::operator=(ByteTrackEngine&&) noexcept = default;

std::vector<ByteTrackEngine::Track> ByteTrackEngine::Update(
    const std::vector<Detection>& detections) {
    return impl_->Update(detections);
}

void ByteTrackEngine::Reset() {
    impl_->Reset();
}

}  // namespace omniface::tracking
