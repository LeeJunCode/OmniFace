#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "omniface/core/detail/onnx_session.h"

namespace omniface {

enum class AnalysisBackend { kOpenFace, kMediaPipe, kL2cs, kMefargAu, kNone };

inline const char* InferenceDeviceToString(InferenceDevice d) {
    switch (d) {
    case InferenceDevice::kCPU:      return "cpu";
    case InferenceDevice::kDirectML: return "directml";
    case InferenceDevice::kCUDA:     return "cuda";
    case InferenceDevice::kTensorRT: return "tensorrt";
    }
    return "cpu";
}

inline InferenceDevice InferenceDeviceFromString(const std::string& s) {
    if (s == "directml") return InferenceDevice::kDirectML;
    if (s == "cuda")     return InferenceDevice::kCUDA;
    if (s == "tensorrt") return InferenceDevice::kTensorRT;
    return InferenceDevice::kCPU;
}

inline const char* AnalysisBackendToString(AnalysisBackend b) {
    switch (b) {
    case AnalysisBackend::kOpenFace:  return "openface";
    case AnalysisBackend::kMediaPipe: return "mediapipe";
    case AnalysisBackend::kL2cs:      return "l2cs";
    case AnalysisBackend::kMefargAu:  return "mefarg_au";
    case AnalysisBackend::kNone:      return "none";
    }
    return "openface";
}

inline AnalysisBackend AnalysisBackendFromString(const std::string& s) {
    if (s == "mediapipe" || s == "onnx") return AnalysisBackend::kMediaPipe;
    if (s == "l2cs" || s == "gaze")     return AnalysisBackend::kL2cs;
    if (s == "mefarg_au")              return AnalysisBackend::kMefargAu;
    if (s == "none")                    return AnalysisBackend::kNone;
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
    InferenceDevice device = InferenceDevice::kCPU;
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

struct L2csConfig {
    std::string model = "mobilenetv2_gaze.onnx";
    bool draw_gaze = true;
    InferenceDevice device = InferenceDevice::kCPU;
};

struct VideoSourceConfig {
    std::string input_path;
    std::string output_path;
};

struct LogConfig {
    std::string level = "debug";
};

// ---- OpenFace 配置（profile + 逐字段覆盖） ------------------
struct OpenFaceConfig {
    std::string profile = "standard";  // "standard" | "max_quality"

    // 逐字段覆盖 — 哨兵值（-1 / -1.0f）表示使用 profile 默认值
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

// ---- MediaPipe 分析配置（FaceMesh 关键点 + 头部姿态） ------------------
struct MediaPipeConfig {
    bool enable_pose = true;
    bool enable_au = false;
    std::string landmark_model = "models/mediapipe/face_mesh_Nx3x192x192_post.onnx";
    std::string canonical_model = "models/mediapipe/canonical_face_model.obj";
    std::string au_model;
    int analysis_threads = 1;
    float margin = 0.25f;
    float score_threshold = 0.95f;
    bool realtime_display = true;
    bool draw_landmarks = true;
    bool draw_face_wireframe = true;
    bool draw_face_normal = true;
    InferenceDevice device = InferenceDevice::kCPU;
};

struct MefargAuConfig {
    std::string model = "models/OpenGprahAU/mefarg_resnet18_au.onnx";
    int intra_threads = 1;
    float threshold = 0.5f;
    bool draw_au = true;
    InferenceDevice device = InferenceDevice::kCPU;
};

struct PipelineConfig {
    ModelConfig model;
    ThresholdConfig thresholds;
    PipelineBehavior behavior;
    VideoSourceConfig video;
    LogConfig log;
    OpenFaceConfig openface;
    MediaPipeConfig onnx;
    L2csConfig l2cs;
    MefargAuConfig mefarg_au;
    AnalysisBackend analysis_backend = AnalysisBackend::kOpenFace;
    std::string output_base_dir;
};

PipelineConfig LoadPipelineConfig(const std::string& yaml_path);
void SavePipelineConfig(const std::string& yaml_path, const PipelineConfig& config);

}  // namespace omniface
