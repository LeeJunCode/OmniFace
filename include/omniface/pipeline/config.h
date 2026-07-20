#pragma once

#include <string>
#include <unordered_map>

namespace omniface::pipeline {

enum class AnalysisBackend {
    kOpenFace,
    kOnnx,
};

struct TrackerConfig {
    int track_buffer = 30;
    float track_thresh = 0.5f;
    float high_thresh = 0.6f;
    float match_thresh = 0.5f;
    float match_thresh_second = 0.3f;
};

struct OpenFaceConfig {
    std::string model_dir = "third_party/OpenFace/build/bin/";
    std::string output_dir = "data/output/openface/";
    std::string output_name = "target_analysis";
    double focal_scale = 500.0;

    bool vis_track = false;
    bool vis_aus = true;
    bool vis_hog = false;
    bool vis_align = false;
};

struct OnnxAnalysisConfig {
    bool enable_pose = true;
    bool enable_gaze = true;
    bool enable_au = true;
    std::string pose_model = "models/mediapipe/face_mesh_Nx3x192x192_post.onnx";
    std::string canonical_model = "models/mediapipe/canonical_face_model.obj";
    std::string gaze_model = "models/l2cs-net/mobilenetv2_gaze.onnx";
    std::string au_model = "models/OpenGprahAU/mefarg_resnet18_au.onnx";
    float pose_margin = 0.25f;
    float pose_score_threshold = 0.95f;
    float gaze_margin = 0.0f;
    float gaze_smooth_alpha = 0.35f;
    float au_margin = 0.0f;
    float au_threshold = 0.5f;
    int analysis_threads = 1;
    bool align_faces = true;
    int au_temporal_window = 5;
    std::unordered_map<std::string, float> au_thresholds;
};

struct AppConfig {
    std::string detector_model = "models/buffalo_sc/det_500m.onnx";
    std::string recognizer_model = "models/buffalo_sc/w600k_mbf.onnx";
    int detection_threads = 2;
    int recognition_threads = 1;
    int detection_input_width = 640;
    int detection_input_height = 640;

    AnalysisBackend backend = AnalysisBackend::kOpenFace;
    OpenFaceConfig openface;
    OnnxAnalysisConfig onnx;

    std::string reference_image;
    int reference_face_index = -1;
    std::string input_video;
    std::string output_video;
    int resume_from_frame = 0;
    std::string output_codec = "avc1";
    std::string tracking_csv;
    std::string onnx_analysis_csv;

    float detection_confidence = 0.5f;
    float detection_nms = 0.4f;
    float match_threshold = 0.4f;
    float recognition_iou = 0.3f;

    int target_recheck_interval = 30;

    TrackerConfig tracker;

    bool show_window = true;
    std::string window_name = "OmniFace";

    int progress_interval = 30;

    std::string server_host = "127.0.0.1";
    int server_port = 8080;
    std::string auth_token;
    int max_upload_mb = 500;
};

class ConfigLoader {
public:
    static AppConfig Load(const std::string& yaml_path);
    static void ApplyOverrides(AppConfig& cfg, int argc, char* argv[]);
};

}  // namespace omniface::pipeline
