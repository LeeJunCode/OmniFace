#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace omniface {

enum class AnalysisBackend { kOpenFace, kOnnx, kNone };

inline const char* AnalysisBackendToString(AnalysisBackend b) {
    switch (b) {
    case AnalysisBackend::kOpenFace: return "openface";
    case AnalysisBackend::kOnnx:     return "onnx";
    case AnalysisBackend::kNone:     return "none";
    }
    return "openface";
}

inline AnalysisBackend AnalysisBackendFromString(const std::string& s) {
    if (s == "onnx")     return AnalysisBackend::kOnnx;
    if (s == "none")     return AnalysisBackend::kNone;
    return AnalysisBackend::kOpenFace;
}

struct ModelConfig {
    std::string detector_model = "models/buffalo_sc/det_500m.onnx";
    std::string recognizer_model = "models/buffalo_sc/w600k_mbf.onnx";
    std::string reference_image;
    int detection_threads = 4;
    int recognition_threads = 4;
    int detection_input_width = 640;
    int detection_input_height = 640;
    float min_face_area = 0.0f;
    float recognition_confidence_threshold = 0.0f;
    float recognition_iou = 0.3f;
    std::string openface_model_dir = "openface_model";
};

struct ThresholdConfig {
    float match_threshold = 0.3f;
    float detection_confidence = 0.5f;
    float detection_nms = 0.4f;
    float tracking_high = 0.5f;
    float tracking_match = 0.4f;
    float tracking_match_second = 0.5f;
    float tracking_new = 0.7f;
};

struct PipelineBehavior {
    float ema_alpha = 0.7f;
    int max_frame_width = 0;
    int target_recheck_interval = 0;
    bool skip_analysis = false;
    bool headless = false;
};

struct VideoSourceConfig {
    std::string input_path;
    std::string output_path;
};

struct LogConfig {
    std::string level = "debug";
};

// ---- OpenFace configuration (profile + per-field overrides) ------------------
struct OpenFaceConfig {
    std::string profile = "standard";  // "standard" | "max_quality"

    // Per-field overrides — sentinel values mean "use profile default"
    std::string model_file;
    std::vector<int> window_sizes_small;
    std::vector<int> window_sizes_init;
    int    optimise_iterations    = -1;
    float  sigma                  = -1.0f;
    float  reg_factor             = -1.0f;
    float  weight_factor          = -1.0f;
    int    use_face_template      = -1;   // -1=unset, 0=false, 1=true
    float  face_template_scale    = -1.0f;
    int    validate_detections    = -1;
    float  validation_boundary    = -1.0f;
    int    refine_hierarchical    = -1;
    int    refine_parameters      = -1;
    int    multi_view_first_frame = -1;
    int    reinit_video_every     = -1;
    int    limit_pose             = -1;
};

// ---- ONNX analysis configuration (MediaPipe FaceMesh + more) ------------------
struct OnnxAnalysisConfig {
    bool enable_pose = true;
    bool enable_gaze = false;
    bool enable_au = false;
    std::string landmark_model = "models/mediapipe/face_mesh_Nx3x192x192_post.onnx";
    std::string canonical_model = "models/mediapipe/canonical_face_model.obj";
    std::string gaze_model;
    std::string au_model;
    int analysis_threads = 1;
    float margin = 0.25f;
    float score_threshold = 0.95f;
    bool realtime_display = true;
    bool draw_landmarks = true;
    bool draw_face_wireframe = true;
    bool draw_face_normal = true;
};

struct PipelineConfig {
    ModelConfig model;
    ThresholdConfig thresholds;
    PipelineBehavior behavior;
    VideoSourceConfig video;
    LogConfig log;
    OpenFaceConfig openface;
    OnnxAnalysisConfig onnx;
    AnalysisBackend analysis_backend = AnalysisBackend::kOpenFace;
    std::string output_base_dir;
};

PipelineConfig LoadPipelineConfig(const std::string& yaml_path);
void SavePipelineConfig(const std::string& yaml_path, const PipelineConfig& config);

}  // namespace omniface
