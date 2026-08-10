#pragma once

#include <fstream>
#include <memory>
#include <string>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include "omniface/core/analysis/interface/analyzer_interface.h"
#include "omniface/core/inference/onnx_session.h"
#include "omniface/types.h"

namespace omniface::analysis::l2cs {

struct L2csEstimationResult {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float yaw_maxp = 0.0f;
    float pitch_maxp = 0.0f;
    cv::Vec3f gaze_vector;
    bool valid = false;
};

class L2csEstimator : public IAnalyzer {
public:
    struct Config {
        std::string model_path;
        int intra_threads = 1;
        InferenceDevice device = InferenceDevice::kCPU;
    };

    explicit L2csEstimator(const Config& cfg);
    ~L2csEstimator();
    L2csEstimator(L2csEstimator&&) noexcept;
    L2csEstimator& operator=(L2csEstimator&&) noexcept;

    L2csEstimator(const L2csEstimator&) = delete;
    L2csEstimator& operator=(const L2csEstimator&) = delete;

    // IAnalyzer interface
    AnalyzerKind Kind() const override { return AnalyzerKind::kL2cs; }
    FrameAnalysisResult Analyze(const cv::Mat& frame, const BoundingBox& bbox,
                                int track_id = -1) override;
    bool IsValid() const override;
    void Reset() override;

    // L2CS-Net native interface（内含 bbox EMA + gaze EMA 平滑）
    L2csEstimationResult Estimate(const cv::Mat& bgr_frame,
                                  const BoundingBox& face_bbox,
                                  int target_id);

    // 视线可视化（静态方法，无状态）
    static void DrawGaze(cv::Mat& frame,
                         const cv::Rect_<float>& target_bbox,
                         const L2csEstimationResult& result);

    // CSV 输出（懒初始化）
    void EnsureCsvOpen(const std::string& output_dir);
    void WriteCsvRow(int frame_index, double timestamp,
                     const L2csEstimationResult& result);
    void CloseCsv();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis::l2cs
