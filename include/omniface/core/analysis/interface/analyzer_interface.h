#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include "omniface/types.h"

namespace omniface::analysis {

// 分析后端枚举
enum class AnalyzerKind { kOpenFace, kMediaPipe, kL2cs, kMefargAu };

inline const char* AnalyzerKindToString(AnalyzerKind k) {
    switch (k) {
    case AnalyzerKind::kOpenFace:  return "openface";
    case AnalyzerKind::kMediaPipe: return "mediapipe";
    case AnalyzerKind::kL2cs:      return "l2cs";
    case AnalyzerKind::kMefargAu:  return "mefarg_au";
    }
    return "unknown";
}

inline AnalyzerKind AnalyzerKindFromString(const std::string& s) {
    if (s == "mediapipe") return AnalyzerKind::kMediaPipe;
    if (s == "l2cs" || s == "gaze") return AnalyzerKind::kL2cs;
    if (s == "mefarg_au") return AnalyzerKind::kMefargAu;
    return AnalyzerKind::kOpenFace;
}

// 统一分析结果 — 各后端填写自己能提供的字段，未提供的保持 nullopt/empty
struct FrameAnalysisResult {
    // 头部姿态（度）
    std::optional<HeadPose> head_pose;
    // 视线（度 + 3D 单位向量）
    std::optional<float> gaze_yaw;
    std::optional<float> gaze_pitch;
    std::optional<cv::Vec3f> gaze_vector;
    // 468 个 2D 关键点（MediaPipe 后端提供）
    std::vector<cv::Point2f> landmarks;
    // OpenFace 特有：视线角度（弧度，OpenFace 坐标系）
    std::optional<float> gaze_angle_x;
    std::optional<float> gaze_angle_y;
    // OpenFace 68 点 2D 关键点
    std::array<float, 136> landmarks_2d{};
    // AU（OpenFace 提供）
    std::vector<std::pair<std::string, double>> action_units;
    // 检测质量
    std::optional<float> quality_score;
    std::optional<int> visible_landmarks;
    // OpenFace 原始数据
    std::optional<float> detection_certainty;
    bool detection_success = false;
    std::optional<float> gaze_0_x, gaze_0_y, gaze_0_z;
    std::optional<float> gaze_1_x, gaze_1_y, gaze_1_z;
    std::optional<float> pose_Tx, pose_Ty, pose_Tz;
    std::array<float, 6> pdm_rigid{};
    std::vector<float> pdm_nonrigid;
    std::vector<float> eye_landmarks_2d;
    std::vector<float> eye_landmarks_3d;
    std::vector<float> landmarks_3d;

    AnalyzerKind backend = AnalyzerKind::kOpenFace;
    bool valid = false;
    int track_id = -1;
};

// 分析后端抽象接口 — 所有后端必须实现
class IAnalyzer {
public:
    virtual ~IAnalyzer() = default;

    virtual AnalyzerKind Kind() const = 0;
    virtual FrameAnalysisResult Analyze(const cv::Mat& frame,
                                         const BoundingBox& bbox,
                                         int track_id = -1) = 0;
    virtual bool IsValid() const = 0;
    virtual void Reset() {}
};

}  // namespace omniface::analysis
