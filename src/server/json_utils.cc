#include <httplib.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "omniface/pipeline/pipeline.h"
#include "omniface/server/web_server.h"

namespace omniface::server {

namespace fs = std::filesystem;

std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                break;
            default:
                out += c;
        }
    }
    return out;
}

bool PathInside(const std::string& path, const std::string& allowed_prefix) {
    if (path.find("..") != std::string::npos) return false;
    return path.rfind(allowed_prefix, 0) == 0;
}

bool ValidTaskId(const std::string& id) {
    if (id.empty() || id.size() > 32) return false;
    for (char c : id)
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '_') return false;
    return true;
}

std::string MakeTaskId() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&t));
    return buf;
}

// ============================================================
// 文件列表 & 指标 JSON 序列化
// ============================================================

std::string ListFilesJson(const std::string& dir, const std::vector<std::string>& exts) {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    if (fs::is_directory(dir)) {
        std::vector<std::string> names;
        for (const auto& e : fs::directory_iterator(dir)) {
            if (!e.is_regular_file() && !e.is_symlink()) continue;
            std::string ext = e.path().extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(c));
            for (const auto& want : exts) {
                if (ext == want) {
                    names.push_back(e.path().generic_string());
                    break;
                }
            }
        }
        std::sort(names.begin(), names.end());
        for (const auto& n : names) {
            if (!first) oss << ",";
            oss << "\"" << JsonEscape(n) << "\"";
            first = false;
        }
    }
    oss << "]";
    return oss.str();
}

std::string MetricsToJson(const pipeline::FrameMetrics& m) {
    std::ostringstream oss;
    oss << "{\"f\":" << m.frame << ",\"found\":" << (m.target_found ? "true" : "false")
        << ",\"tid\":" << m.target_track_id << ",\"sim\":" << m.similarity
        << ",\"pose_valid\":" << (m.pose_valid ? "true" : "false") << ",\"pose\":[" << m.pitch
        << "," << m.yaw << "," << m.roll << "]"
        << ",\"gaze_valid\":" << (m.gaze_valid ? "true" : "false") << ",\"gaze\":[" << m.gaze_yaw
        << "," << m.gaze_pitch << "]" << ",\"aus\":[";
    for (size_t i = 0; i < m.aus.size(); ++i) {
        if (i) oss << ",";
        oss << "[\"" << JsonEscape(m.aus[i].first) << "\"," << m.aus[i].second << "]";
    }
    oss << "],\"tracks\":[";
    for (size_t i = 0; i < m.tracks.size(); ++i) {
        if (i) oss << ",";
        oss << "[" << m.tracks[i].id << "," << m.tracks[i].similarity << ","
            << (m.tracks[i].is_target ? 1 : 0) << "]";
    }
    oss << "]}";
    return oss.str();
}

const char* StateName(int s) {
    switch (s) {
        case 0:
            return "idle";
        case 1:
            return "loading";
        case 2:
            return "running";
        case 3:
            return "finished";
        default:
            return "error";
    }
}

// ============================================================
// 表单参数 → 配置覆盖
// ============================================================

void ApplyParams(const httplib::Request& req, pipeline::AppConfig& cfg) {
    auto get = [&](const char* key, const std::string& def) {
        return req.has_param(key) ? req.get_param_value(key) : def;
    };
    auto get_f = [&](const char* key, float def, float lo = 0.0f, float hi = 1.0f) {
        try {
            if (!req.has_param(key)) return def;
            float v = std::stof(req.get_param_value(key));
            return std::clamp(v, lo, hi);
        } catch (...) {
            return def;
        }
    };
    auto get_i = [&](const char* key, int def, int lo = 1, int hi = 999) {
        try {
            if (!req.has_param(key)) return def;
            int v = std::stoi(req.get_param_value(key));
            return std::clamp(v, lo, hi);
        } catch (...) {
            return def;
        }
    };
    auto get_b = [&](const char* key, bool def) {
        return req.has_param(key) ? (req.get_param_value(key) == "true") : def;
    };

    cfg.reference_image = get("reference", cfg.reference_image);
    cfg.reference_face_index = get_i("reference_face_index", cfg.reference_face_index, -1, 99);
    cfg.input_video = get("video", cfg.input_video);
    cfg.backend = (get("backend", "") == "onnx") ? pipeline::AnalysisBackend::kOnnx
                                                 : pipeline::AnalysisBackend::kOpenFace;
    cfg.match_threshold = get_f("match_threshold", cfg.match_threshold);
    cfg.onnx.enable_pose = get_b("enable_pose", cfg.onnx.enable_pose);
    cfg.onnx.enable_gaze = get_b("enable_gaze", cfg.onnx.enable_gaze);
    cfg.onnx.enable_au = get_b("enable_au", cfg.onnx.enable_au);
    cfg.detection_confidence = get_f("detection_confidence", cfg.detection_confidence);
    cfg.detection_nms = get_f("detection_nms", cfg.detection_nms);
    cfg.recognition_iou = get_f("recognition_iou", cfg.recognition_iou);
    cfg.target_recheck_interval =
        get_i("target_recheck_interval", cfg.target_recheck_interval, 0, 999);
    cfg.tracker.track_buffer = get_i("track_buffer", cfg.tracker.track_buffer, 1, 999);
    cfg.tracker.track_thresh = get_f("tracking_high", cfg.tracker.track_thresh);
    cfg.tracker.match_thresh = get_f("tracking_match", cfg.tracker.match_thresh);
    cfg.tracker.match_thresh_second =
        get_f("tracking_match_second", cfg.tracker.match_thresh_second);
    cfg.tracker.high_thresh = get_f("tracking_new", cfg.tracker.high_thresh);
    cfg.onnx.pose_margin = get_f("pose_margin", cfg.onnx.pose_margin, 0.0f, 1.0f);
    cfg.onnx.pose_score_threshold = get_f("pose_score_threshold", cfg.onnx.pose_score_threshold);
    cfg.onnx.gaze_margin = get_f("gaze_margin", cfg.onnx.gaze_margin, 0.0f, 1.0f);
    cfg.onnx.gaze_smooth_alpha =
        get_f("gaze_smooth_alpha", cfg.onnx.gaze_smooth_alpha, 0.01f, 1.0f);
    cfg.onnx.au_margin = get_f("au_margin", cfg.onnx.au_margin, 0.0f, 1.0f);
    cfg.onnx.au_threshold = get_f("au_threshold", cfg.onnx.au_threshold, -1.0f, 1.0f);
}

}  // namespace omniface::server
