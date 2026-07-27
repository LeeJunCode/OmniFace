#include "omniface/pipeline/config.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace omniface {

namespace {

template <typename T>
T SafeGet(const YAML::Node& node, const std::string& key, T default_val) {
    if (node && node[key] && node[key].IsScalar()) {
        return node[key].as<T>();
    }
    return default_val;
}

YAML::Node SafeSub(const YAML::Node& node, const std::string& key) {
    if (node && node[key]) return node[key];
    return YAML::Node{};
}

}  // namespace

PipelineConfig LoadPipelineConfig(const std::string& yaml_path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(yaml_path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Failed to parse config file " + yaml_path + ": " + e.what());
    }

    if (!root.IsMap()) {
        throw std::runtime_error("Config root must be a map: " + yaml_path);
    }

    PipelineConfig cfg;

    if (auto m = SafeSub(root, "model")) {
        cfg.model.detector_model =
            SafeGet<std::string>(m, "detector_model", cfg.model.detector_model);
        cfg.model.recognizer_model =
            SafeGet<std::string>(m, "recognizer_model", cfg.model.recognizer_model);
        cfg.model.reference_image =
            SafeGet<std::string>(m, "reference_image", cfg.model.reference_image);
        cfg.model.detection_threads =
            SafeGet<int>(m, "detection_threads", cfg.model.detection_threads);
        cfg.model.recognition_threads =
            SafeGet<int>(m, "recognition_threads", cfg.model.recognition_threads);
        cfg.model.detection_input_width =
            SafeGet<int>(m, "detection_input_width", cfg.model.detection_input_width);
        cfg.model.detection_input_height =
            SafeGet<int>(m, "detection_input_height", cfg.model.detection_input_height);
        cfg.model.min_face_area =
            SafeGet<float>(m, "min_face_area", cfg.model.min_face_area);
        cfg.model.recognition_confidence_threshold =
            SafeGet<float>(m, "recognition_confidence_threshold",
                           cfg.model.recognition_confidence_threshold);
        cfg.model.recognition_iou =
            SafeGet<float>(m, "recognition_iou", cfg.model.recognition_iou);
        cfg.model.openface_model_dir =
            SafeGet<std::string>(m, "openface_model_dir", cfg.model.openface_model_dir);
    }

    if (auto t = SafeSub(root, "thresholds")) {
        cfg.thresholds.match_threshold =
            SafeGet<float>(t, "match_threshold", cfg.thresholds.match_threshold);
        cfg.thresholds.detection_confidence =
            SafeGet<float>(t, "detection_confidence", cfg.thresholds.detection_confidence);
        cfg.thresholds.detection_nms =
            SafeGet<float>(t, "detection_nms", cfg.thresholds.detection_nms);
        cfg.thresholds.tracking_high =
            SafeGet<float>(t, "tracking_high", cfg.thresholds.tracking_high);
        cfg.thresholds.tracking_match =
            SafeGet<float>(t, "tracking_match", cfg.thresholds.tracking_match);
        cfg.thresholds.tracking_match_second =
            SafeGet<float>(t, "tracking_match_second", cfg.thresholds.tracking_match_second);
        cfg.thresholds.tracking_new =
            SafeGet<float>(t, "tracking_new", cfg.thresholds.tracking_new);
    }

    if (auto b = SafeSub(root, "behavior")) {
        cfg.behavior.ema_alpha = SafeGet<float>(b, "ema_alpha", cfg.behavior.ema_alpha);
        cfg.behavior.max_frame_width =
            SafeGet<int>(b, "max_frame_width", cfg.behavior.max_frame_width);
        cfg.behavior.target_recheck_interval =
            SafeGet<int>(b, "target_recheck_interval", cfg.behavior.target_recheck_interval);
        cfg.behavior.headless =
            SafeGet<bool>(b, "headless", cfg.behavior.headless);
    }

    if (auto v = SafeSub(root, "video")) {
        cfg.video.input_path = SafeGet<std::string>(v, "input_path", cfg.video.input_path);
        cfg.video.output_path = SafeGet<std::string>(v, "output_path", cfg.video.output_path);
    }

    if (auto l = SafeSub(root, "log")) {
        cfg.log.level = SafeGet<std::string>(l, "level", cfg.log.level);
    }

    // Sanitize: clamp values to valid ranges
    cfg.model.detection_threads = std::clamp(cfg.model.detection_threads, 1, 32);
    cfg.model.recognition_threads = std::clamp(cfg.model.recognition_threads, 1, 32);
    cfg.model.detection_input_width = std::max(cfg.model.detection_input_width, 64);
    cfg.model.detection_input_height = std::max(cfg.model.detection_input_height, 64);
    cfg.model.min_face_area = std::max(cfg.model.min_face_area, 0.0f);
    cfg.model.recognition_confidence_threshold = std::clamp(cfg.model.recognition_confidence_threshold, 0.0f, 1.0f);
    cfg.model.recognition_iou = std::clamp(cfg.model.recognition_iou, 0.0f, 1.0f);

    cfg.thresholds.match_threshold = std::clamp(cfg.thresholds.match_threshold, 0.0f, 1.0f);
    cfg.thresholds.detection_confidence = std::clamp(cfg.thresholds.detection_confidence, 0.0f, 1.0f);
    cfg.thresholds.detection_nms = std::clamp(cfg.thresholds.detection_nms, 0.0f, 1.0f);
    cfg.thresholds.tracking_high = std::clamp(cfg.thresholds.tracking_high, 0.0f, 1.0f);
    cfg.thresholds.tracking_match = std::clamp(cfg.thresholds.tracking_match, 0.0f, 1.0f);
    cfg.thresholds.tracking_match_second = std::clamp(cfg.thresholds.tracking_match_second, 0.0f, 1.0f);
    cfg.thresholds.tracking_new = std::clamp(cfg.thresholds.tracking_new, 0.0f, 1.0f);

    cfg.behavior.ema_alpha = std::clamp(cfg.behavior.ema_alpha, 0.0f, 1.0f);
    cfg.behavior.max_frame_width = std::max(cfg.behavior.max_frame_width, 0);
    cfg.behavior.target_recheck_interval = std::clamp(cfg.behavior.target_recheck_interval, 1, 300);

    return cfg;
}

void SavePipelineConfig(const std::string& yaml_path, const PipelineConfig& cfg) {
    YAML::Node root;

    root["model"]["detector_model"] = cfg.model.detector_model;
    root["model"]["recognizer_model"] = cfg.model.recognizer_model;
    root["model"]["reference_image"] = cfg.model.reference_image;
    root["model"]["detection_threads"] = cfg.model.detection_threads;
    root["model"]["recognition_threads"] = cfg.model.recognition_threads;
    root["model"]["detection_input_width"] = cfg.model.detection_input_width;
    root["model"]["detection_input_height"] = cfg.model.detection_input_height;
    root["model"]["min_face_area"] = cfg.model.min_face_area;
    root["model"]["recognition_confidence_threshold"] = cfg.model.recognition_confidence_threshold;
    root["model"]["recognition_iou"] = cfg.model.recognition_iou;
    root["model"]["openface_model_dir"] = cfg.model.openface_model_dir;

    root["thresholds"]["match_threshold"] = cfg.thresholds.match_threshold;
    root["thresholds"]["detection_confidence"] = cfg.thresholds.detection_confidence;
    root["thresholds"]["detection_nms"] = cfg.thresholds.detection_nms;
    root["thresholds"]["tracking_high"] = cfg.thresholds.tracking_high;
    root["thresholds"]["tracking_match"] = cfg.thresholds.tracking_match;
    root["thresholds"]["tracking_match_second"] = cfg.thresholds.tracking_match_second;
    root["thresholds"]["tracking_new"] = cfg.thresholds.tracking_new;

    root["behavior"]["ema_alpha"] = cfg.behavior.ema_alpha;
    root["behavior"]["max_frame_width"] = cfg.behavior.max_frame_width;
    root["behavior"]["target_recheck_interval"] = cfg.behavior.target_recheck_interval;
    root["behavior"]["headless"] = cfg.behavior.headless;

    root["video"]["input_path"] = cfg.video.input_path;
    root["video"]["output_path"] = cfg.video.output_path;

    root["log"]["level"] = cfg.log.level;

    std::ofstream fout(yaml_path);
    if (!fout.is_open()) {
        throw std::runtime_error("Failed to write config: " + yaml_path);
    }
    fout << root;
}

}  // namespace omniface
