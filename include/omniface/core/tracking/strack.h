#pragma once

#include <Eigen/StdVector>
#include <array>
#include <vector>

#include "omniface/core/tracking/kalman_filter.h"
#include "omniface/types.h"

namespace omniface::tracking {

enum class STrackState { kNew = 0, kTracked = 1, kLost = 2, kRemoved = 3 };

class STrack {
public:
    using List = std::vector<STrack, Eigen::aligned_allocator<STrack>>;

    STrack() = default;

    STrack(const std::array<float, 4>& tlwh, float score);
    STrack(std::array<float, 4>&& tlwh, float score);

    bool IsActivated() const {
        return is_activated_;
    }
    int TrackId() const {
        return track_id_;
    }
    STrackState State() const {
        return state_;
    }
    float Score() const {
        return score_;
    }
    int HitStreak() const {
        return hit_streak_;
    }
    int MissStreak() const {
        return miss_streak_;
    }

    const std::array<float, 4>& Tlwh() const {
        return tlwh_;
    }
    std::array<float, 4> Tlbr() const;
    const KalMean& Mean() const {
        return mean_;
    }
    const KalCova& Covariance() const {
        return covariance_;
    }

    static std::array<float, 4> TlwhToXyah(const std::array<float, 4>& tlwh);
    std::array<float, 4> ToXyah() const;

    void Activate(const KalmanFilter& kalman_filter, int frame_id);
    void ReActivate(const STrack& new_track, int frame_id, const KalmanFilter& kalman_filter,
                    bool new_id = false);
    void UpdateTrack(const STrack& new_track, int frame_id, const KalmanFilter& kalman_filter);
    void MarkLost();
    void MarkRemoved();
    int EndFrame() const {
        return frame_id_;
    }

    void Predict(const KalmanFilter& kalman_filter);

    static void MultiPredict(List& tracks, const KalmanFilter& kalman_filter);

    float ComputeIoU(const STrack& other) const;

    static std::vector<float> IoUDistance(const std::vector<const STrack*>& tracks,
                                          const List& detections,
                                          const KalmanFilter& kalman_filter);

private:
    void UpdateTlwhFromState();

    static int NextId();

    std::array<float, 4> tlwh_{};
    float score_ = 0.0f;
    int track_id_ = 0;
    int frame_id_ = 0;
    int start_frame_ = 0;
    int tracklet_len_ = 0;
    int miss_streak_ = 0;
    int hit_streak_ = 0;

    STrackState state_ = STrackState::kNew;
    bool is_activated_ = false;

    KalMean mean_;
    KalCova covariance_;

public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

}  // namespace omniface::tracking
