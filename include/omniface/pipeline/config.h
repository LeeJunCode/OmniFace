#pragma once

#include <cstddef>
#include <string>

namespace omniface {

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

struct PipelineConfig {
    ModelConfig model;
    ThresholdConfig thresholds;
    PipelineBehavior behavior;
    VideoSourceConfig video;
    LogConfig log;
    std::string output_base_dir;
};

PipelineConfig LoadPipelineConfig(const std::string& yaml_path);
void SavePipelineConfig(const std::string& yaml_path, const PipelineConfig& config);

}  // namespace omniface
