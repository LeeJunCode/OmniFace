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
    // Quality indicators
    float quality_score = 0.0f;   // 0=worst, 1=best
    int visible_landmarks = 0;    // non-occluded landmark count (0..68)

    // ---- FeatureExtraction-compatible fields ----
    float detection_certainty = 0.0f;  // raw -1..1 from CLNF validator
    bool detection_success = false;

    // Gaze direction vectors (world coordinates, mm)
    float gaze_0_x = 0, gaze_0_y = 0, gaze_0_z = 0;
    float gaze_1_x = 0, gaze_1_y = 0, gaze_1_z = 0;

    // Head pose translation (camera coordinates, mm)
    float pose_Tx = 0, pose_Ty = 0, pose_Tz = 0;

    // PDM rigid parameters: scale, Rx, Ry, Rz, Tx, Ty
    std::array<float, 6> pdm_rigid{};

    // PDM non-rigid shape parameters
    std::vector<float> pdm_nonrigid;

    // Eye-region 2D landmarks (56 points, pixel coords: x0..x55, y0..y55)
    std::vector<float> eye_landmarks_2d;

    // Eye-region 3D landmarks (56 points, world mm: X0..X55, Y0..Y55, Z0..Z55)
    std::vector<float> eye_landmarks_3d;

    // Face 3D landmarks (68 points, world mm: X0..X67, Y0..Y67, Z0..Z67)
    std::vector<float> landmarks_3d;

    // AU classifications (binary presence, 0/1)
    std::vector<std::pair<std::string, double>> au_classifications;
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
