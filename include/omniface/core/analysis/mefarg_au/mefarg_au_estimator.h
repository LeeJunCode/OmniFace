#pragma once

#include <array>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include "omniface/core/analysis/analyzer_interface.h"
#include "omniface/core/detail/onnx_session.h"

namespace omniface::analysis::mefarg_au {

// 41 个 AU 的完整名称表（27 主 + 14 双侧）
inline constexpr int kNumAuOutputs = 41;
inline constexpr int kNumMainAu = 27;
inline constexpr int kNumSubAu = 14;

inline constexpr std::array<const char*, kNumAuOutputs> kAuNames = {
    "AU1", "AU2", "AU4", "AU5", "AU6", "AU7", "AU9", "AU10",
    "AU11", "AU12", "AU13", "AU14", "AU15", "AU16", "AU17", "AU18",
    "AU19", "AU20", "AU22", "AU23", "AU24", "AU25", "AU26", "AU27",
    "AU32", "AU38", "AU39",
    "AUL1", "AUR1", "AUL2", "AUR2", "AUL4", "AUR4", "AUL6", "AUR6",
    "AUL10", "AUR10", "AUL12", "AUR12", "AUL14", "AUR14"
};

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
