#pragma once

#include <memory>
#include <vector>

namespace omniface::tracking {

class ByteTrackEngine {
public:
    struct Config {
        int track_buffer = 30;
        float track_thresh = 0.5f;
        float high_thresh = 0.6f;
        float match_thresh = 0.8f;
        float match_thresh_second = 0.5f;
    };

    struct Detection {
        float tlwh[4];
        float score;
    };

    struct Track {
        int id;
        float tlwh[4];
        float score;
        bool is_activated;
        int hit_streak;
        int miss_streak;
    };

    ByteTrackEngine();
    explicit ByteTrackEngine(Config config);
    ~ByteTrackEngine();
    ByteTrackEngine(ByteTrackEngine&&) noexcept;
    ByteTrackEngine& operator=(ByteTrackEngine&&) noexcept;

    std::vector<Track> Update(const std::vector<Detection>& detections);

    void Reset();

private:
    ByteTrackEngine(const ByteTrackEngine&) = delete;
    ByteTrackEngine& operator=(const ByteTrackEngine&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::tracking
