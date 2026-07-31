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

    if (auto of = SafeSub(root, "openface")) {
        cfg.openface.profile =
            SafeGet<std::string>(of, "profile", cfg.openface.profile);
        if (of["model_file"])
            cfg.openface.model_file = of["model_file"].as<std::string>();
        if (of["sigma"])
            cfg.openface.sigma = of["sigma"].as<float>();
        if (of["reg_factor"])
            cfg.openface.reg_factor = of["reg_factor"].as<float>();
        if (of["weight_factor"])
            cfg.openface.weight_factor = of["weight_factor"].as<float>();
        if (of["optimise_iterations"])
            cfg.openface.optimise_iterations = of["optimise_iterations"].as<int>();
        if (of["use_face_template"])
            cfg.openface.use_face_template = of["use_face_template"].as<bool>() ? 1 : 0;
        if (of["face_template_scale"])
            cfg.openface.face_template_scale = of["face_template_scale"].as<float>();
        if (of["validate_detections"])
            cfg.openface.validate_detections = of["validate_detections"].as<bool>() ? 1 : 0;
        if (of["validation_boundary"])
            cfg.openface.validation_boundary = of["validation_boundary"].as<float>();
        if (of["refine_hierarchical"])
            cfg.openface.refine_hierarchical = of["refine_hierarchical"].as<bool>() ? 1 : 0;
        if (of["refine_parameters"])
            cfg.openface.refine_parameters = of["refine_parameters"].as<bool>() ? 1 : 0;
        if (of["multi_view_first_frame"])
            cfg.openface.multi_view_first_frame = of["multi_view_first_frame"].as<bool>() ? 1 : 0;
        if (of["reinit_video_every"])
            cfg.openface.reinit_video_every = of["reinit_video_every"].as<int>();
        if (of["limit_pose"])
            cfg.openface.limit_pose = of["limit_pose"].as<bool>() ? 1 : 0;
        if (of["window_sizes_small"] && of["window_sizes_small"].IsSequence())
            cfg.openface.window_sizes_small = of["window_sizes_small"].as<std::vector<int>>();
        if (of["window_sizes_init"] && of["window_sizes_init"].IsSequence())
            cfg.openface.window_sizes_init = of["window_sizes_init"].as<std::vector<int>>();
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

    cfg.analysis_backend = AnalysisBackendFromString(
        SafeGet<std::string>(root, "analysis_backend", "openface"));

    if (auto onx = SafeSub(root, "onnx")) {
        cfg.onnx.enable_pose =
            SafeGet<bool>(onx, "enable_pose", cfg.onnx.enable_pose);
        cfg.onnx.enable_au =
            SafeGet<bool>(onx, "enable_au", cfg.onnx.enable_au);
        cfg.onnx.landmark_model =
            SafeGet<std::string>(onx, "landmark_model", cfg.onnx.landmark_model);
        cfg.onnx.canonical_model =
            SafeGet<std::string>(onx, "canonical_model", cfg.onnx.canonical_model);
        cfg.onnx.au_model =
            SafeGet<std::string>(onx, "au_model", cfg.onnx.au_model);
        cfg.onnx.analysis_threads =
            SafeGet<int>(onx, "analysis_threads", cfg.onnx.analysis_threads);
        cfg.onnx.margin =
            SafeGet<float>(onx, "margin", cfg.onnx.margin);
        cfg.onnx.score_threshold =
            SafeGet<float>(onx, "score_threshold", cfg.onnx.score_threshold);
        cfg.onnx.realtime_display =
            SafeGet<bool>(onx, "realtime_display", cfg.onnx.realtime_display);
        cfg.onnx.draw_landmarks =
            SafeGet<bool>(onx, "draw_landmarks", cfg.onnx.draw_landmarks);
        cfg.onnx.draw_face_wireframe =
            SafeGet<bool>(onx, "draw_face_wireframe", cfg.onnx.draw_face_wireframe);
        cfg.onnx.draw_face_normal =
            SafeGet<bool>(onx, "draw_face_normal", cfg.onnx.draw_face_normal);
    }

    if (auto g = SafeSub(root, "l2cs")) {
        cfg.l2cs.model =
            SafeGet<std::string>(g, "model", cfg.l2cs.model);
        cfg.l2cs.draw_gaze =
            SafeGet<bool>(g, "draw_gaze", cfg.l2cs.draw_gaze);
    }
    // 向后兼容旧配置键名 "gaze"
    if (auto g = SafeSub(root, "gaze")) {
        cfg.l2cs.model =
            SafeGet<std::string>(g, "model", cfg.l2cs.model);
        cfg.l2cs.draw_gaze =
            SafeGet<bool>(g, "draw_gaze", cfg.l2cs.draw_gaze);
    }

    // 值域裁剪，确保配置值在合法范围内
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

    cfg.onnx.analysis_threads = std::clamp(cfg.onnx.analysis_threads, 1, 32);
    cfg.onnx.margin = std::clamp(cfg.onnx.margin, 0.0f, 1.0f);
    cfg.onnx.score_threshold = std::clamp(cfg.onnx.score_threshold, 0.0f, 1.0f);

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

    {
        const auto& of = cfg.openface;
        root["openface"]["profile"] = of.profile;
        if (!of.model_file.empty())
            root["openface"]["model_file"] = of.model_file;
        if (of.sigma >= 0.0f)
            root["openface"]["sigma"] = of.sigma;
        if (of.reg_factor >= 0.0f)
            root["openface"]["reg_factor"] = of.reg_factor;
        if (of.weight_factor >= 0.0f)
            root["openface"]["weight_factor"] = of.weight_factor;
        if (of.optimise_iterations >= 0)
            root["openface"]["optimise_iterations"] = of.optimise_iterations;
        if (of.use_face_template >= 0)
            root["openface"]["use_face_template"] = (of.use_face_template != 0);
        if (of.face_template_scale >= 0.0f)
            root["openface"]["face_template_scale"] = of.face_template_scale;
        if (of.validate_detections >= 0)
            root["openface"]["validate_detections"] = (of.validate_detections != 0);
        if (of.validation_boundary >= 0.0f)
            root["openface"]["validation_boundary"] = of.validation_boundary;
        if (of.refine_hierarchical >= 0)
            root["openface"]["refine_hierarchical"] = (of.refine_hierarchical != 0);
        if (of.refine_parameters >= 0)
            root["openface"]["refine_parameters"] = (of.refine_parameters != 0);
        if (of.multi_view_first_frame >= 0)
            root["openface"]["multi_view_first_frame"] = (of.multi_view_first_frame != 0);
        if (of.reinit_video_every >= 0)
            root["openface"]["reinit_video_every"] = of.reinit_video_every;
        if (of.limit_pose >= 0)
            root["openface"]["limit_pose"] = (of.limit_pose != 0);
        if (!of.window_sizes_small.empty())
            root["openface"]["window_sizes_small"] = of.window_sizes_small;
        if (!of.window_sizes_init.empty())
            root["openface"]["window_sizes_init"] = of.window_sizes_init;
    }

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

    root["analysis_backend"] = AnalysisBackendToString(cfg.analysis_backend);

    {
        const auto& onx = cfg.onnx;
        root["onnx"]["enable_pose"] = onx.enable_pose;
        root["onnx"]["enable_au"] = onx.enable_au;
        root["onnx"]["landmark_model"] = onx.landmark_model;
        root["onnx"]["canonical_model"] = onx.canonical_model;
        root["onnx"]["au_model"] = onx.au_model;
        root["onnx"]["analysis_threads"] = onx.analysis_threads;
        root["onnx"]["margin"] = onx.margin;
        root["onnx"]["score_threshold"] = onx.score_threshold;
        root["onnx"]["realtime_display"] = onx.realtime_display;
        root["onnx"]["draw_landmarks"] = onx.draw_landmarks;
        root["onnx"]["draw_face_wireframe"] = onx.draw_face_wireframe;
        root["onnx"]["draw_face_normal"] = onx.draw_face_normal;
    }

    {
        const auto& l2 = cfg.l2cs;
        root["l2cs"]["model"] = l2.model;
        root["l2cs"]["draw_gaze"] = l2.draw_gaze;
    }

    std::ofstream fout(yaml_path);
    if (!fout.is_open()) {
        throw std::runtime_error("Failed to write config: " + yaml_path);
    }
    fout << root;
}

}  // namespace omniface
