#include "omniface/core/tracking/strack.h"

#include <algorithm>
#include <atomic>

namespace omniface::tracking {

STrack::STrack(const std::array<float, 4>& tlwh, float score)
    : tlwh_(tlwh), score_(score), state_(STrackState::kNew) {}

STrack::STrack(std::array<float, 4>&& tlwh, float score)
    : tlwh_(std::move(tlwh)), score_(score), state_(STrackState::kNew) {}

std::array<float, 4> STrack::Tlbr() const {
    return {tlwh_[0], tlwh_[1], tlwh_[0] + tlwh_[2], tlwh_[1] + tlwh_[3]};
}

std::array<float, 4> STrack::TlwhToXyah(const std::array<float, 4>& tlwh) {
    return {tlwh[0] + tlwh[2] / 2.0f, tlwh[1] + tlwh[3] / 2.0f, tlwh[2] / tlwh[3], tlwh[3]};
}

std::array<float, 4> STrack::ToXyah() const {
    return TlwhToXyah(tlwh_);
}

void STrack::UpdateTlwhFromState() {
    float cx = mean_(0);
    float cy = mean_(1);
    float a = mean_(2);
    float h = mean_(3);

    float w = a * h;
    tlwh_[0] = cx - w / 2.0f;
    tlwh_[1] = cy - h / 2.0f;
    tlwh_[2] = w;
    tlwh_[3] = h;
}

int STrack::NextId() {
    static std::atomic<int> count{0};
    return ++count;
}

void STrack::Activate(const KalmanFilter& kalman_filter, int frame_id) {
    track_id_ = NextId();

    auto xyah = ToXyah();
    DetectBox xyah_box;
    xyah_box << xyah[0], xyah[1], xyah[2], xyah[3];
    auto [m, cov] = kalman_filter.Initiate(xyah_box);
    mean_ = m;
    covariance_ = cov;

    UpdateTlwhFromState();
    tracklet_len_ = 0;
    hit_streak_ = 0;
    miss_streak_ = 0;
    state_ = STrackState::kTracked;
    if (frame_id == 1) {
        is_activated_ = true;
    }
    frame_id_ = frame_id;
    start_frame_ = frame_id;
}

void STrack::ReActivate(const STrack& new_track, int frame_id, const KalmanFilter& kalman_filter,
                        bool new_id) {
    auto xyah = new_track.ToXyah();
    DetectBox xyah_box;
    xyah_box << xyah[0], xyah[1], xyah[2], xyah[3];
    auto [m, cov] = kalman_filter.Update(mean_, covariance_, xyah_box);
    mean_ = m;
    covariance_ = cov;

    UpdateTlwhFromState();
    tracklet_len_ = 0;
    hit_streak_ = 1;
    miss_streak_ = 0;
    state_ = STrackState::kTracked;
    is_activated_ = true;
    frame_id_ = frame_id;
    score_ = new_track.score_;

    if (new_id) {
        track_id_ = NextId();
    }
}

void STrack::UpdateTrack(const STrack& new_track, int frame_id, const KalmanFilter& kalman_filter) {
    frame_id_ = frame_id;
    tracklet_len_++;
    hit_streak_++;
    miss_streak_ = 0;

    auto xyah = new_track.ToXyah();
    DetectBox xyah_box;
    xyah_box << xyah[0], xyah[1], xyah[2], xyah[3];
    auto [m, cov] = kalman_filter.Update(mean_, covariance_, xyah_box);
    mean_ = m;
    covariance_ = cov;

    UpdateTlwhFromState();
    state_ = STrackState::kTracked;
    is_activated_ = true;
    score_ = new_track.score_;
}

void STrack::MarkLost() {
    state_ = STrackState::kLost;
    hit_streak_ = 0;
    miss_streak_++;
}

void STrack::MarkRemoved() {
    state_ = STrackState::kRemoved;
}

void STrack::Predict(const KalmanFilter& kalman_filter) {
    if (state_ != STrackState::kTracked) {
        mean_(7) = 0.0f;
    }
    kalman_filter.Predict(mean_, covariance_);
    UpdateTlwhFromState();
}

void STrack::MultiPredict(List& tracks, const KalmanFilter& kalman_filter) {
    for (auto& track : tracks) {
        if (track.state_ != STrackState::kTracked) {
            track.mean_(7) = 0.0f;
        }
        kalman_filter.Predict(track.mean_, track.covariance_);
        track.UpdateTlwhFromState();
    }
}

float STrack::ComputeIoU(const STrack& other) const {
    return ::omniface::ComputeIoU(Tlbr(), other.Tlbr());
}

std::vector<float> STrack::IoUDistance(const std::vector<const STrack*>& tracks,
                                       const List& detections, const KalmanFilter& kalman_filter) {
    int t_size = static_cast<int>(tracks.size());
    int d_size = static_cast<int>(detections.size());

    std::vector<float> cost_matrix(static_cast<size_t>(d_size * t_size), 1.0f);

    for (int d = 0; d < d_size; d++) {
        auto det_tlbr = detections[d].Tlbr();
        float area_d = (det_tlbr[2] - det_tlbr[0]) * (det_tlbr[3] - det_tlbr[1]);
        if (area_d <= 0.0f) continue;

        auto det_xyah = TlwhToXyah(detections[d].Tlwh());
        DetectBox det_box;
        det_box << det_xyah[0], det_xyah[1], det_xyah[2], det_xyah[3];

        for (int t = 0; t < t_size; t++) {
            float gate =
                kalman_filter.GatingDistance(tracks[t]->Mean(), tracks[t]->Covariance(), det_box);
            if (gate > KalmanFilter::kChi2Inv95[4]) continue;

            auto track_tlbr = tracks[t]->Tlbr();
            float area_t = (track_tlbr[2] - track_tlbr[0]) * (track_tlbr[3] - track_tlbr[1]);
            if (area_t <= 0.0f) continue;

            float iou = ::omniface::ComputeIoU(track_tlbr, det_tlbr);
            cost_matrix[d * t_size + t] = 1.0f - iou;
        }
    }
    return cost_matrix;
}

}  // namespace omniface::tracking
