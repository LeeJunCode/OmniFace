#pragma once

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/core/analysis/interface/analyzer_interface.h"
#include "omniface/core/analysis/openface/openface_writer.h"
#include "omniface/types.h"

namespace omniface {
struct OpenFaceConfig;
}

namespace omniface::analysis::openface {

enum class OpenFaceProfile {
    kStandard,
    kMaxQuality,
};

struct OpenFaceParams {
    std::string model_file;
    std::string mtcnn_detector_path;

    std::vector<int> window_sizes_small = {0, 9, 7, 0};
    std::vector<int> window_sizes_init  = {11, 9, 7, 5};

    int   num_optimisation_iteration = 5;
    float sigma        = 1.5f;
    float reg_factor   = 25.0f;
    float weight_factor = 0.0f;

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

OpenFaceParams MakeProfileParams(OpenFaceProfile profile,
                                 const std::string& model_dir);

void ApplyOverrides(OpenFaceParams& params, const OpenFaceConfig& config,
                    const std::string& model_dir = "");

OpenFaceProfile ParseProfile(const std::string& name);

class OpenFaceAnalyzer : public IAnalyzer {
public:
    struct Config {
        std::string model_dir;
        float min_face_aspect   = 0.35f;
        int   min_face_width_px = 40;
        OpenFaceParams openface;
    };

    explicit OpenFaceAnalyzer(const Config& cfg);
    ~OpenFaceAnalyzer();

    OpenFaceAnalyzer(const OpenFaceAnalyzer&) = delete;
    OpenFaceAnalyzer& operator=(const OpenFaceAnalyzer&) = delete;

    // IAnalyzer interface
    AnalyzerKind Kind() const override { return AnalyzerKind::kOpenFace; }
    FrameAnalysisResult Analyze(const cv::Mat& frame, const BoundingBox& bbox,
                                int track_id = -1) override;
    bool IsValid() const override;
    void Reset() override;

    // OpenFace 原生接口（返回旧 FaceAnalysisResult，内嵌可视化）
    FaceAnalysisResult Analyze(cv::Mat& frame, const cv::Rect_<float>& bbox);

    cv::Mat GetAlignedFace() const;

    bool CanAlignFace() const;

    void GetHOG(cv::Mat_<double>& hog, int& num_rows, int& num_cols) const;

    // Writer management
    void EnsureWriterInitialized(const std::string& input_path,
                                 const std::string& output_dir,
                                 double fps, bool output_aligned);
    void WriteObservation(const FaceAnalysisResult& analysis,
                          int frame_index, double fps,
                          const cv::Mat& aligned_face);
    void CloseWriter();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis::openface
