#include "omniface/pipeline/config.h"

#include <yaml-cpp/yaml.h>

namespace omniface::pipeline {

namespace {

template <typename T>
T SafeGet(const YAML::Node& node, const std::string& key, T default_val) {
    if (!node || !node[key]) return default_val;
    try {
        return node[key].as<T>();
    } catch (const YAML::BadConversion&) {
        return default_val;
    }
}

#define LOAD_SAME(node, field) cfg.field = SafeGet<decltype(cfg.field)>(node, #field, cfg.field)
#define LOAD_FIELD(node, key, field) field = SafeGet<decltype(field)>(node, key, field)
}  // namespace

AppConfig ConfigLoader::Load(const std::string& yaml_path) {
    AppConfig cfg;
    YAML::Node root;
    try {
        root = YAML::LoadFile(yaml_path);
    } catch (const std::exception& e) {
        return cfg;
    }

    if (auto m = root["model"]) {
        cfg.detector_model = SafeGet<std::string>(m, "detector", cfg.detector_model);
        cfg.recognizer_model = SafeGet<std::string>(m, "recognizer", cfg.recognizer_model);
        LOAD_SAME(m, detection_threads);
        LOAD_SAME(m, recognition_threads);
        LOAD_SAME(m, detection_input_width);
        LOAD_SAME(m, detection_input_height);
    }

    if (auto a = root["analysis"]) {
        std::string be = SafeGet<std::string>(a, "backend", "openface");
        cfg.backend = (be == "onnx") ? AnalysisBackend::kOnnx : AnalysisBackend::kOpenFace;

        if (auto of = a["openface"]) {
            LOAD_FIELD(of, "model_dir", cfg.openface.model_dir);
            LOAD_FIELD(of, "output_dir", cfg.openface.output_dir);
            LOAD_FIELD(of, "output_name", cfg.openface.output_name);
            LOAD_FIELD(of, "focal_scale", cfg.openface.focal_scale);
            LOAD_FIELD(of, "vis_track", cfg.openface.vis_track);
            LOAD_FIELD(of, "vis_aus", cfg.openface.vis_aus);
            LOAD_FIELD(of, "vis_hog", cfg.openface.vis_hog);
            LOAD_FIELD(of, "vis_align", cfg.openface.vis_align);
        }
        if (auto on = a["onnx"]) {
            LOAD_FIELD(on, "enable_pose", cfg.onnx.enable_pose);
            LOAD_FIELD(on, "enable_gaze", cfg.onnx.enable_gaze);
            LOAD_FIELD(on, "enable_au", cfg.onnx.enable_au);
            LOAD_FIELD(on, "analysis_threads", cfg.onnx.analysis_threads);
            LOAD_FIELD(on, "align_faces", cfg.onnx.align_faces);
            LOAD_FIELD(on, "au_temporal_window", cfg.onnx.au_temporal_window);
            if (auto p = on["pose"]) {
                LOAD_FIELD(p, "model", cfg.onnx.pose_model);
                LOAD_FIELD(p, "canonical", cfg.onnx.canonical_model);
                LOAD_FIELD(p, "margin", cfg.onnx.pose_margin);
                LOAD_FIELD(p, "score_threshold", cfg.onnx.pose_score_threshold);
            }
            if (auto g = on["gaze"]) {
                LOAD_FIELD(g, "model", cfg.onnx.gaze_model);
                LOAD_FIELD(g, "margin", cfg.onnx.gaze_margin);
                LOAD_FIELD(g, "smooth_alpha", cfg.onnx.gaze_smooth_alpha);
            }
            if (auto u = on["au"]) {
                LOAD_FIELD(u, "model", cfg.onnx.au_model);
                LOAD_FIELD(u, "margin", cfg.onnx.au_margin);
                LOAD_FIELD(u, "threshold", cfg.onnx.au_threshold);
                if (auto th = u["thresholds"]) {
                    for (auto it = th.begin(); it != th.end(); ++it) {
                        try {
                            cfg.onnx.au_thresholds[it->first.as<std::string>()] =
                                it->second.as<float>();
                        } catch (const YAML::BadConversion&) {
                        }
                    }
                }
            }
        }
    }

    if (auto i = root["input"]) {
        LOAD_SAME(i, reference_image);
        LOAD_FIELD(i, "video", cfg.input_video);
    }
    if (auto o = root["output"]) {
        LOAD_FIELD(o, "video", cfg.output_video);
        LOAD_SAME(o, output_codec);
        LOAD_SAME(o, tracking_csv);
    }

    if (auto t = root["thresholds"]) {
        LOAD_SAME(t, detection_confidence);
        LOAD_SAME(t, detection_nms);
        LOAD_SAME(t, match_threshold);
        LOAD_SAME(t, recognition_iou);
        LOAD_SAME(t, target_recheck_interval);
        LOAD_SAME(t, min_face_area);
        LOAD_SAME(t, detection_interval);
        LOAD_SAME(t, max_frame_width);
        LOAD_SAME(t, async_recognition);
    }

    if (auto tr = root["tracking"]) {
        LOAD_FIELD(tr, "track_buffer", cfg.tracker.track_buffer);
        LOAD_FIELD(tr, "high_threshold", cfg.tracker.track_thresh);
        LOAD_FIELD(tr, "match_threshold", cfg.tracker.match_thresh);
        LOAD_FIELD(tr, "match_threshold_second", cfg.tracker.match_thresh_second);
        LOAD_FIELD(tr, "new_track_threshold", cfg.tracker.high_thresh);
    }

    if (auto v = root["visualization"]) {
        LOAD_SAME(v, show_window);
        LOAD_SAME(v, window_name);
    }
    if (auto l = root["log"]) {
        LOAD_SAME(l, progress_interval);
    }
    if (auto s = root["server"]) {
        LOAD_SAME(s, server_host);
        LOAD_FIELD(s, "port", cfg.server_port);
        LOAD_FIELD(s, "auth_token", cfg.auth_token);
        LOAD_FIELD(s, "max_upload_mb", cfg.max_upload_mb);
    }

    return cfg;
}

void ConfigLoader::ApplyOverrides(AppConfig& cfg, int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--reference" && i + 1 < argc) {
            cfg.reference_image = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            cfg.input_video = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            cfg.output_video = argv[++i];
        } else if (arg == "--backend" && i + 1 < argc) {
            std::string backend = argv[++i];
            cfg.backend = (backend == "onnx") ? AnalysisBackend::kOnnx : AnalysisBackend::kOpenFace;
        }
    }
}

}  // namespace omniface::pipeline
