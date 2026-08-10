#pragma once

#include <array>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include "omniface/core/analysis/interface/analyzer_interface.h"
#include "omniface/core/analysis/mefarg_au/mefarg_au_constants.h"
#include "omniface/core/inference/onnx_session.h"

namespace omniface::analysis::mefarg_au {

struct MefargAuEstimationResult {
    bool valid = false;
    std::array<float, kNumAuOutputs> au_logits{};
    std::array<float, kNumAuOutputs> au_probs{};
    std::array<bool, kNumAuOutputs> au_active{};
};

class MefargAuEstimator : public IAnalyzer {
public:
    struct Config {
        std::string model = "models/OpenGprahAU/mefarg_resnet18_au.onnx";
        int intra_threads = 1;
        float threshold = 0.5f;
        InferenceDevice device = InferenceDevice::kCPU;
    };

    explicit MefargAuEstimator(const Config& cfg);
    ~MefargAuEstimator() override;
    MefargAuEstimator(MefargAuEstimator&&) noexcept;
    MefargAuEstimator& operator=(MefargAuEstimator&&) noexcept;

    AnalyzerKind Kind() const override { return AnalyzerKind::kMefargAu; }
    FrameAnalysisResult Analyze(const cv::Mat& frame,
                                 const BoundingBox& bbox,
                                 int track_id = -1) override;
    bool IsValid() const override;

    MefargAuEstimationResult Estimate(const cv::Mat& bgr_frame,
                                       const BoundingBox& face_bbox);

    void EnsureCsvOpen(const std::string& output_dir);
    void WriteCsvRow(int frame_index, double timestamp,
                     const MefargAuEstimationResult& result);
    void CloseCsv();

    static void DrawAu(cv::Mat& frame,
                       const cv::Rect_<float>& target_bbox,
                       const MefargAuEstimationResult& result,
                       float threshold = 0.5f);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis::mefarg_au
