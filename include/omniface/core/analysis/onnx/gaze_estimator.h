#pragma once

#include <memory>
#include <string>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include "omniface/types.h"

namespace omniface::analysis::onnx {

struct GazeEstimationResult {
    float yaw = 0.0f;          // 偏航角（度），正值 = 目标看向右侧
    float pitch = 0.0f;        // 俯仰角（度），正值 = 目标看向上方
    cv::Vec3f gaze_vector;     // 3D 单位向量（相机坐标系：+X 右，+Y 下，+Z 前）
    bool valid = false;
};

// L2CS-Net 视线估计（MobileOne-S0 骨干）
// 输入：人脸裁剪 RGB 448x448，ImageNet 归一化
// 输出：偏航/俯仰角（度）+ 3D 视线方向向量
class GazeEstimator {
public:
    struct Config {
        std::string model_path;
        int intra_threads = 1;
    };

    explicit GazeEstimator(const Config& cfg);
    ~GazeEstimator();
    GazeEstimator(GazeEstimator&&) noexcept;
    GazeEstimator& operator=(GazeEstimator&&) noexcept;

    GazeEstimator(const GazeEstimator&) = delete;
    GazeEstimator& operator=(const GazeEstimator&) = delete;

    GazeEstimationResult Estimate(const cv::Mat& bgr_frame, const BoundingBox& face_bbox);
    bool IsValid() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis::onnx
