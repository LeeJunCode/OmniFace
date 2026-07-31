#pragma once

#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

namespace omniface::analysis::onnx {

struct PoseEstimationResult;

// 头部姿态可视化工具（纯静态方法，与估计器解耦）
class PoseVisualizer {
public:
    // 绘制 468 个人脸网格关键点（绿色圆点）
    static void DrawLandmarks(cv::Mat& image,
                              const std::vector<cv::Point2f>& landmarks);

    // 绘制投影后的 3D 人脸线框（脸型轮廓、眼、眉、鼻、唇）
    static void DrawProjectedModel(cv::Mat& image,
                                   const PoseEstimationResult& result,
                                   const std::vector<cv::Point3f>& canonical_model);

    // 从鼻尖位置绘制面部法线箭头（品红色）
    static void DrawFaceNormal(cv::Mat& image,
                               const PoseEstimationResult& result,
                               float face_size,
                               const cv::Point3f& nose_canonical);
};

}  // namespace omniface::analysis::onnx
