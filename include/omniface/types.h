#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace omniface {

// Face detection bounding box with 5-point landmarks
struct BoundingBox {
    float x1, y1, x2, y2;
    float confidence;
    std::array<float, 10> keypoints{};  // 5 points: (x,y) alternating

    float operator[](int i) const { return (&x1)[i]; }
    float& operator[](int i) { return (&x1)[i]; }
};

// 512-D face feature embedding (MobileFaceNet)
struct FaceEmbedding {
    std::array<float, 512> features{};
    bool valid = false;
};

// Tracked face info from ByteTrack
struct TrackedFace {
    int id;
    BoundingBox box;
    int consecutive_hit_count;
    int consecutive_miss_count;
};

// Head pose angles (degrees)
struct HeadPose {
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;
};

// Per-frame facial analysis result from OpenFace
struct FaceAnalysisResult {
    bool valid = false;
    HeadPose head_pose;
    float gaze_angle_x = 0.0f;  // radians
    float gaze_angle_y = 0.0f;  // radians
    // 68-point 2D landmarks (x1..x68, y1..y68)
    std::array<float, 136> landmarks_2d{};
    // AU names and their intensity values
    std::vector<std::pair<std::string, double>> action_units;
};

// Cosine similarity match result
struct MatchResult {
    BoundingBox box;
    float similarity = 0.0f;
    bool is_match = false;
    int tracked_id = -1;
    bool valid = false;
};

// Generic IoU for any box type supporting operator[](int)
template <typename BoxA, typename BoxB>
inline float ComputeIoU(const BoxA& a, const BoxB& b) noexcept {
    float inter_x1 = std::max(a[0], b[0]);
    float inter_y1 = std::max(a[1], b[1]);
    float inter_x2 = std::min(a[2], b[2]);
    float inter_y2 = std::min(a[3], b[3]);
    if (inter_x2 <= inter_x1 || inter_y2 <= inter_y1) return 0.0f;
    float inter = (inter_x2 - inter_x1) * (inter_y2 - inter_y1);
    float area_a = (a[2] - a[0]) * (a[3] - a[1]);
    float area_b = (b[2] - b[0]) * (b[3] - b[1]);
    float denom = area_a + area_b - inter;
    return (denom > 1e-8f) ? inter / denom : 0.0f;
}

}  // namespace omniface
