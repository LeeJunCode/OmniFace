#pragma once

#include <array>
#include <memory>
#include <opencv2/core/mat.hpp>
#include <string>
#include <unordered_map>

#include "omniface/types.h"

namespace omniface::analysis {

constexpr std::array<const char*, 27> kMainAuNames = {
    "AU1",  "AU2",  "AU4",  "AU5",  "AU6",  "AU7",  "AU9",  "AU10", "AU11",
    "AU12", "AU13", "AU14", "AU15", "AU16", "AU17", "AU18", "AU19", "AU20",
    "AU22", "AU23", "AU24", "AU25", "AU26", "AU27", "AU32", "AU38", "AU39"};

constexpr std::array<const char*, 14> kSubAuNames = {
    "AU1_L", "AU1_R",  "AU2_L",  "AU2_R",  "AU4_L",  "AU4_R",  "AU6_L",
    "AU6_R", "AU10_L", "AU10_R", "AU12_L", "AU12_R", "AU14_L", "AU14_R"};

struct AuResult {
    float cosine_sim = 0.0f;
    bool active = false;
};

struct AuAnalysisResult {
    std::array<AuResult, 27> main_aus;
    std::array<AuResult, 14> sub_aus;
    bool valid = false;
};

class AuAnalyzer {
public:
    struct Config {
        float margin = 0.0f;
        float threshold = 0.5f;
        int intra_threads = 1;

        std::unordered_map<std::string, float> per_au_thresholds;
    };

    explicit AuAnalyzer(const std::string& model_path, Config config);
    ~AuAnalyzer();
    AuAnalyzer(AuAnalyzer&&) noexcept;
    AuAnalyzer& operator=(AuAnalyzer&&) noexcept;

    AuAnalysisResult Analyze(const cv::Mat& bgr_image, const BoundingBox& face_bbox);

    AuAnalysisResult AnalyzeFromAligned(const cv::Mat& aligned_face_bgr);

private:
    AuAnalyzer(const AuAnalyzer&) = delete;
    AuAnalyzer& operator=(const AuAnalyzer&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis
