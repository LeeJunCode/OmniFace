#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace omniface {

// 边界框: 左上角(x1,y1) + 右下角(x2,y2) + 置信度
// keypoints[10]: 5个关键点 (x0,y0, x1,y1, ..., x4,y4) — 左眼/右眼/鼻尖/左嘴角/右嘴角
// operator[]利用内存连续性将x1/y1/x2/y2作为数组访问 (用于ComputeIoU等通用接口)
struct BoundingBox {
    float x1, y1, x2, y2;
    float confidence;
    std::array<float, 10> keypoints{};

    float operator[](int i) const {
        switch (i) {
        case 0: return x1;
        case 1: return y1;
        case 2: return x2;
        case 3: return y2;
        default: return 0.0f;
        }
    }
    float& operator[](int i) {
        switch (i) {
        case 0: return x1;
        case 1: return y1;
        case 2: return x2;
        case 3: return y2;
        default: return x1;
        }
    }
};

// 512维人脸特征向量 (ArcFace/InsightFace标准输出维度)，valid标识是否成功提取
struct FaceEmbedding {
    std::array<float, 512> features{};
    bool valid = false;
};

// 跟踪输出: 包含轨道ID、当前边界框、连续命中/丢失计数
// consecutive_hit_count: 用于判断跟踪稳定性(通常>3帧才算稳定)
// consecutive_miss_count: 用于清理短暂丢失的轨道
struct TrackedFace {
    int id;
    BoundingBox box;
    int consecutive_hit_count;
    int consecutive_miss_count;
};

struct HeadPose {
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;
};

struct FaceAnalysisResult {
    bool valid = false;
    HeadPose head_pose;
    float gaze_angle_x = 0.0f;
    float gaze_angle_y = 0.0f;
    std::array<float, 136> landmarks_2d{};
    std::vector<std::pair<std::string, double>> action_units;
    float quality_score = 0.0f;
    int visible_landmarks = 0;

    float detection_certainty = 0.0f;
    bool detection_success = false;

    float gaze_0_x = 0, gaze_0_y = 0, gaze_0_z = 0;
    float gaze_1_x = 0, gaze_1_y = 0, gaze_1_z = 0;

    float pose_Tx = 0, pose_Ty = 0, pose_Tz = 0;

    std::array<float, 6> pdm_rigid{};

    std::vector<float> pdm_nonrigid;

    std::vector<float> eye_landmarks_2d;

    std::vector<float> eye_landmarks_3d;

    std::vector<float> landmarks_3d;

    std::vector<std::pair<std::string, double>> au_classifications;
};

// 人脸匹配结果: similarity为余弦相似度([-1,1])，is_match表示是否超过设定阈值
struct MatchResult {
    BoundingBox box;
    float similarity = 0.0f;
    bool is_match = false;
    int tracked_id = -1;
    bool valid = false;
};

// 通用IoU计算: 模板化支持所有提供operator[]的框类型(如BoundingBox, STrack等)
// 公式: IoU = 交集面积 / 并集面积，其中并集 = A面积 + B面积 - 交集面积
// 1e-8: 防止面积为零时的除零错误
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
    // 1e-8: 处理极端情况(两个框面积都为零)
    return (denom > 1e-8f) ? inter / denom : 0.0f;
}

}  // namespace omniface
