#pragma once

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/types.h"

namespace omniface {
struct OpenFaceConfig;
}

namespace omniface::analysis {

// ---- Preset profiles ---------------------------------------------------------
enum class OpenFaceProfile {
    kStandard,    // matches OpenFaceOffline.exe defaults
    kMaxQuality,  // higher accuracy, slower
};

// ---- Resolved runtime parameters (no optionals) ------------------------------
struct OpenFaceParams {
    std::string model_file;           // absolute path to main_ceclm_general.txt
    std::string mtcnn_detector_path;  // absolute path to MTCNN_detector.txt

    std::vector<int> window_sizes_small = {0, 9, 7, 0};
    std::vector<int> window_sizes_init  = {11, 9, 7, 5};

    int   num_optimisation_iteration = 5;
    float sigma        = 1.5f;
    float reg_factor   = 25.0f;
    float weight_factor = 0.0f;  // 0 = standard RLMS, >0 = NU-RLMS

    bool  use_face_template   = false;
    float face_template_scale = 0.3f;

    bool  validate_detections  = true;
    float validation_boundary  = 0.725f;

    bool refine_hierarchical = true;
    bool refine_parameters   = true;

    bool multi_view_first_frame = false;
    int  reinit_video_every     = 2;
    bool limit_pose             = true;
};

// ---- Factory functions -------------------------------------------------------
OpenFaceParams MakeProfileParams(OpenFaceProfile profile,
                                 const std::string& model_dir);

void ApplyOverrides(OpenFaceParams& params, const OpenFaceConfig& config,
                     const std::string& model_dir = "");

OpenFaceProfile ParseProfile(const std::string& name);

// ---- FaceAnalyzer ------------------------------------------------------------
class FaceAnalyzer {
public:
    struct Config {
        std::string model_dir;
        float min_face_aspect   = 0.35f;
        int   min_face_width_px = 40;
        OpenFaceParams openface;
    };

    explicit FaceAnalyzer(const Config& cfg);
    ~FaceAnalyzer();

    FaceAnalyzer(const FaceAnalyzer&) = delete;
    FaceAnalyzer& operator=(const FaceAnalyzer&) = delete;

    FaceAnalysisResult Analyze(cv::Mat& frame, const cv::Rect_<float>& bbox);

    bool IsValid() const;

    cv::Mat GetAlignedFace() const;

    bool CanAlignFace() const;

    void GetHOG(cv::Mat_<double>& hog, int& num_rows, int& num_cols) const;

    void Reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis
