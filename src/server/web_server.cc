#include "omniface/server/web_server.h"

#include <httplib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <sstream>

#include "omniface/pipeline/pipeline.h"

namespace omniface::server {

namespace fs = std::filesystem;
using namespace std::chrono_literals;

WebServer::WebServer(pipeline::AppConfig base_cfg) : base_cfg_(std::move(base_cfg)) {}

WebServer::~WebServer() {
    stop_requested_ = true;
    JoinWorker();
}

void WebServer::JoinWorker() {
    if (worker_.joinable()) worker_.join();
}

std::string WebServer::StatusJson() const {
    std::lock_guard<std::mutex> lock(meta_mutex_);
    std::ostringstream oss;
    oss << "{\"state\":\"" << StateName(static_cast<int>(state_.load())) << "\""
        << ",\"task_id\":\"" << task_id_ << "\"" << ",\"frame\":" << current_frame_.load()
        << ",\"total_frames\":" << total_frames_.load()
        << ",\"target_track_id\":" << target_track_id_.load()
        << ",\"target_found\":" << (target_track_id_.load() > 0 ? "true" : "false")
        << ",\"fps\":" << fps_.load() << ",\"paused\":" << (paused_.load() ? "true" : "false")
        << ",\"error\":\"" << JsonEscape(error_message_) << "\"" << "}";
    return oss.str();
}

bool WebServer::CheckAuth(const httplib::Request& req, httplib::Response& res) const {
    const std::string& token = base_cfg_.auth_token;
    if (token.empty()) return true;
    if (req.get_param_value("token") == token) return true;
    res.status = 401;
    res.set_content("{\"error\":\"认证失败: 需要有效的 token 参数\"}", "application/json");
    return false;
}

void WebServer::RegisterFileRoutes(httplib::Server& svr) {
    svr.Get("/api/files", [this](const httplib::Request& req, httplib::Response& res) {
        if (!CheckAuth(req, res)) return;
        std::string json = "{\"images\":" + ListFilesJson("data/img", {".jpg", ".jpeg", ".png"}) +
                           ",\"videos\":" + ListFilesJson("data/video", {".mp4", ".avi", ".mkv"}) +
                           "}";
        res.set_content(json, "application/json");
    });

    svr.Delete("/api/files", [this](const httplib::Request& req, httplib::Response& res) {
        if (!CheckAuth(req, res)) return;
        std::string path = req.get_param_value("path");
        if (!PathInside(path, "data/img/") && !PathInside(path, "data/video/")) {
            res.status = 400;
            res.set_content("{\"error\":\"路径不合法\"}", "application/json");
            return;
        }
        fs::remove(path);
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/api/upload", [this](const httplib::Request& req, httplib::Response& res) {
        if (!CheckAuth(req, res)) return;
        if (!req.has_file("file")) {
            res.status = 400;
            res.set_content("{\"error\":\"缺少文件\"}", "application/json");
            return;
        }
        const auto& file = req.get_file_value("file");
        std::string type = req.has_param("type") ? req.get_param_value("type")
                                                 : req.get_file_value("type").content;
        std::string dir = (type == "video") ? "data/video/" : "data/img/";

        std::string name = fs::path(file.filename).filename().string();
        if (name.empty() || name.find("..") != std::string::npos) {
            res.status = 400;
            res.set_content("{\"error\":\"文件名不合法\"}", "application/json");
            return;
        }
        fs::create_directories(dir);
        std::ofstream out(dir + name, std::ios::binary);
        if (!out.is_open()) {
            res.status = 500;
            res.set_content("{\"error\":\"无法写入文件\"}", "application/json");
            return;
        }
        out.write(file.content.data(), static_cast<std::streamsize>(file.content.size()));
        out.close();
        res.set_content("{\"ok\":true,\"path\":\"" + JsonEscape(dir + name) + "\"}",
                        "application/json");
    });

    svr.Post("/api/detect_face", [this](const httplib::Request& req, httplib::Response& res) {
        std::string image = req.get_param_value("image");
        if (!PathInside(image, "data/")) {
            res.status = 400;
            res.set_content("{\"error\":\"路径不合法\"}", "application/json");
            return;
        }
        cv::Mat img = cv::imread(image);
        if (img.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"无法读取图片\"}", "application/json");
            return;
        }
        std::vector<omniface::BoundingBox> dets;
        {
            std::lock_guard<std::mutex> lock(detector_mutex_);
            if (!probe_detector_) {
                try {
                    detection::FaceDetector::Config dc;
                    dc.confidence_threshold = base_cfg_.detection_confidence;
                    dc.nms_threshold = base_cfg_.detection_nms;
                    dc.input_width = base_cfg_.detection_input_width;
                    dc.input_height = base_cfg_.detection_input_height;
                    dc.intra_threads = 1;
                    probe_detector_ =
                        std::make_unique<detection::FaceDetector>(base_cfg_.detector_model, dc);
                } catch (const std::exception& e) {
                    res.status = 500;
                    res.set_content(
                        "{\"error\":\"检测器初始化失败: " + JsonEscape(e.what()) + "\"}",
                        "application/json");
                    return;
                }
            }
            dets = probe_detector_->Detect(img);
        }
        std::sort(dets.begin(), dets.end(),
                  [](const auto& a, const auto& b) { return a.confidence > b.confidence; });

        std::ostringstream oss;
        oss << "{\"width\":" << img.cols << ",\"height\":" << img.rows << ",\"faces\":[";
        for (size_t i = 0; i < dets.size(); ++i) {
            if (i) oss << ",";
            oss << "{\"index\":" << i << ",\"confidence\":" << dets[i].confidence
                << ",\"x1\":" << dets[i].x1 << ",\"y1\":" << dets[i].y1 << ",\"x2\":" << dets[i].x2
                << ",\"y2\":" << dets[i].y2 << "}";
        }
        oss << "]}";
        res.set_content(oss.str(), "application/json");
    });
}

void WebServer::RegisterPresetRoutes(httplib::Server& svr) {
    svr.Get("/api/presets", [](const httplib::Request&, httplib::Response& res) {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        if (fs::is_directory("data/presets")) {
            for (const auto& e : fs::directory_iterator("data/presets")) {
                if (e.path().extension() != ".json") continue;
                std::ifstream f(e.path());
                std::stringstream content;
                content << f.rdbuf();
                if (!first) oss << ",";
                oss << "\"" << JsonEscape(e.path().stem().string()) << "\":" << content.str();
                first = false;
            }
        }
        oss << "}";
        res.set_content(oss.str(), "application/json");
    });

    svr.Post("/api/presets", [this](const httplib::Request& req, httplib::Response& res) {
        if (!CheckAuth(req, res)) return;
        std::string name = req.get_param_value("name");
        if (name.empty() || name.find('/') != std::string::npos ||
            name.find("..") != std::string::npos) {
            res.status = 400;
            res.set_content("{\"error\":\"预设名不合法\"}", "application/json");
            return;
        }
        fs::create_directories("data/presets");
        std::ofstream f("data/presets/" + name + ".json");
        f << "{";
        bool first = true;
        for (const auto& [k, v] : req.params) {
            if (k == "name") continue;
            if (!first) f << ",";
            f << "\"" << JsonEscape(k) << "\":\"" << JsonEscape(v) << "\"";
            first = false;
        }
        f << "}";
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Delete("/api/presets", [this](const httplib::Request& req, httplib::Response& res) {
        if (!CheckAuth(req, res)) return;
        std::string name = req.get_param_value("name");
        if (!name.empty() && name.find('/') == std::string::npos &&
            name.find("..") == std::string::npos) {
            fs::remove("data/presets/" + name + ".json");
        }
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Get("/api/config", [this](const httplib::Request&, httplib::Response& res) {
        const auto& c = base_cfg_;
        std::ostringstream oss;
        oss << "{\"reference\":\"" << JsonEscape(c.reference_image) << "\"" << ",\"video\":\""
            << JsonEscape(c.input_video) << "\"" << ",\"backend\":\""
            << (c.backend == pipeline::AnalysisBackend::kOnnx ? "onnx" : "openface") << "\""
            << ",\"match_threshold\":" << c.match_threshold
            << ",\"enable_pose\":" << (c.onnx.enable_pose ? "true" : "false")
            << ",\"enable_gaze\":" << (c.onnx.enable_gaze ? "true" : "false")
            << ",\"enable_au\":" << (c.onnx.enable_au ? "true" : "false")
            << ",\"detection_confidence\":" << c.detection_confidence
            << ",\"detection_nms\":" << c.detection_nms
            << ",\"recognition_iou\":" << c.recognition_iou
            << ",\"track_buffer\":" << c.tracker.track_buffer
            << ",\"tracking_high\":" << c.tracker.track_thresh
            << ",\"tracking_match\":" << c.tracker.match_thresh
            << ",\"tracking_match_second\":" << c.tracker.match_thresh_second
            << ",\"tracking_new\":" << c.tracker.high_thresh
            << ",\"pose_margin\":" << c.onnx.pose_margin
            << ",\"pose_score_threshold\":" << c.onnx.pose_score_threshold
            << ",\"gaze_margin\":" << c.onnx.gaze_margin
            << ",\"gaze_smooth_alpha\":" << c.onnx.gaze_smooth_alpha
            << ",\"au_margin\":" << c.onnx.au_margin << ",\"au_threshold\":" << c.onnx.au_threshold
            << "}";
        res.set_content(oss.str(), "application/json");
    });
}

void WebServer::RegisterControlRoutes(httplib::Server& svr) {
    svr.Post("/api/start", [this](const httplib::Request& req, httplib::Response& res) {
        if (!CheckAuth(req, res)) return;
        if (state_ == State::kLoading || state_ == State::kRunning) {
            res.status = 409;
            res.set_content("{\"error\":\"已有分析任务在运行\"}", "application/json");
            return;
        }
        JoinWorker();

        pipeline::AppConfig cfg = base_cfg_;
        ApplyParams(req, cfg);
        cfg.show_window = false;
        cfg.openface.vis_track = cfg.openface.vis_aus = false;
        cfg.openface.vis_hog = cfg.openface.vis_align = false;

        if (req.has_param("resume_task")) {
            std::string old_id = req.get_param_value("resume_task");
            if (ValidTaskId(old_id)) {
                std::ifstream old_json("data/output/" + old_id + "/task.json");
                if (old_json.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(old_json)),
                                        std::istreambuf_iterator<char>());
                    auto pos = content.find("\"frames\":");
                    if (pos != std::string::npos) {
                        int last_frame = std::atoi(content.c_str() + pos + 9);
                        if (last_frame > 0) {
                            cfg.resume_from_frame = last_frame;
                        }
                    }
                }
            }
        }

        std::string task_id = MakeTaskId();
        std::string task_dir = "data/output/" + task_id + "/";
        fs::create_directories(task_dir);
        bool save_video =
            !req.has_param("save_video") || req.get_param_value("save_video") == "true";
        bool save_csv = !req.has_param("save_csv") || req.get_param_value("save_csv") == "true";
        cfg.output_video = save_video ? task_dir + "annotated.mp4" : "";
        cfg.tracking_csv = save_csv ? task_dir + "tracking.csv" : "";
        cfg.onnx_analysis_csv =
            (cfg.backend == pipeline::AnalysisBackend::kOnnx) ? task_dir + "onnx_analysis.csv" : "";
        cfg.openface.output_dir = task_dir + "openface/";

        std::map<std::string, std::string> raw_params;
        for (const auto& [k, v] : req.params) raw_params[k] = v;

        {
            std::lock_guard<std::mutex> lock(meta_mutex_);
            task_id_ = task_id;
            active_cfg_ = cfg;
            active_params_ = raw_params;
            error_message_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_json_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            latest_jpeg_.clear();
            latest_raw_jpeg_.clear();
            frame_seq_ = 0;
        }
        stop_requested_ = false;
        paused_ = false;
        step_once_ = false;
        current_frame_ = 0;
        total_frames_ = 0;
        target_track_id_ = -1;
        state_ = State::kLoading;
        worker_ = std::thread(&WebServer::WorkerLoop, this, cfg, task_id, raw_params);

        res.set_content("{\"ok\":true,\"task_id\":\"" + task_id + "\"}", "application/json");
    });

    svr.Post("/api/stop", [this](const httplib::Request& req, httplib::Response& res) {
        if (!CheckAuth(req, res)) return;
        stop_requested_ = true;
        paused_ = false;
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/api/pause", [this](const httplib::Request&, httplib::Response& res) {
        paused_ = !paused_;
        res.set_content(std::string("{\"paused\":") + (paused_ ? "true" : "false") + "}",
                        "application/json");
    });

    svr.Post("/api/step", [this](const httplib::Request&, httplib::Response& res) {
        step_once_ = true;
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(StatusJson(), "application/json");
    });

    svr.Get("/api/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        size_t since = 0;
        try {
            if (req.has_param("since")) since = std::stoul(req.get_param_value("since"));
        } catch (...) {
        }
        std::ostringstream oss;
        oss << "[";
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            for (size_t i = since; i < metrics_json_.size(); ++i) {
                if (i > since) oss << ",";
                oss << metrics_json_[i];
            }
        }
        oss << "]";
        res.set_content(oss.str(), "application/json");
    });
}

void WebServer::RegisterTaskRoutes(httplib::Server& svr) {
    svr.Get("/api/tasks", [](const httplib::Request&, httplib::Response& res) {
        std::vector<std::string> entries;
        if (fs::is_directory("data/output")) {
            for (const auto& e : fs::directory_iterator("data/output")) {
                auto meta = e.path() / "task.json";
                if (!fs::is_regular_file(meta)) continue;
                std::ifstream f(meta);
                std::stringstream content;
                content << f.rdbuf();
                entries.push_back(content.str());
            }
        }
        std::sort(entries.rbegin(), entries.rend());
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i) oss << ",";
            oss << entries[i];
        }
        oss << "]";
        res.set_content(oss.str(), "application/json");
    });

    svr.Get(R"(/api/tasks/([0-9_]+)/zip)", [](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        if (!ValidTaskId(id) || !fs::is_directory("data/output/" + id)) {
            res.status = 404;
            return;
        }
        std::string zip_path = "data/output/" + id + ".zip";
        if (!fs::is_regular_file(zip_path)) {
            pid_t pid = fork();
            if (pid == 0) {
                if (chdir("data/output") != 0) _exit(1);
                std::string zip_name = id + ".zip";
                const char* argv[] = {"zip", "-qr", zip_name.c_str(), id.c_str(), nullptr};
                execvp("zip", const_cast<char* const*>(argv));
                _exit(1);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                if (status != 0) {
                    fs::remove(zip_path);
                    res.status = 500;
                    res.set_content("{\"error\":\"打包失败 — 请确认系统已安装 zip 命令\"}",
                                    "application/json");
                    return;
                }
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"打包失败 — 系统资源不足\"}", "application/json");
                return;
            }
        }
        res.set_redirect("/data/output/" + id + ".zip");
    });

    svr.Delete(
        R"(/api/tasks/([0-9_]+))", [this](const httplib::Request& req, httplib::Response& res) {
            if (!CheckAuth(req, res)) return;
            std::string id = req.matches[1];
            {
                std::lock_guard<std::mutex> lock(meta_mutex_);
                if (id == task_id_ && (state_ == State::kRunning || state_ == State::kLoading)) {
                    res.status = 409;
                    res.set_content("{\"error\":\"任务正在运行\"}", "application/json");
                    return;
                }
            }
            if (ValidTaskId(id)) {
                fs::remove_all("data/output/" + id);
                fs::remove("data/output/" + id + ".zip");
            }
            res.set_content("{\"ok\":true}", "application/json");
        });
}

void WebServer::RegisterStreamRoutes(httplib::Server& svr) {
    auto make_stream_handler = [this](bool raw) {
        return [this, raw](const httplib::Request&, httplib::Response& res) {
            res.set_chunked_content_provider(
                "multipart/x-mixed-replace; boundary=omniframe",
                [this, raw, last_seq = uint64_t{0}, idle_rounds = 0](
                    size_t, httplib::DataSink& sink) mutable {
                    std::string payload;
                    for (int i = 0; i < 50 && payload.empty(); ++i) {
                        {
                            std::lock_guard<std::mutex> lock(frame_mutex_);
                            const auto& buf = raw ? latest_raw_jpeg_ : latest_jpeg_;
                            bool fresh = frame_seq_ != last_seq;
                            if (!buf.empty() && (fresh || idle_rounds >= 2)) {
                                last_seq = frame_seq_;
                                std::ostringstream head;
                                head << "--omniframe\r\nContent-Type: image/jpeg\r\n"
                                     << "Content-Length: " << buf.size() << "\r\n\r\n";
                                payload = head.str();
                                payload.append(reinterpret_cast<const char*>(buf.data()),
                                               buf.size());
                                payload += "\r\n";
                            }
                        }
                        if (payload.empty()) std::this_thread::sleep_for(20ms);
                    }
                    if (!payload.empty()) {
                        idle_rounds = 0;
                        return sink.write(payload.data(), payload.size());
                    }
                    idle_rounds++;
                    State st = state_;
                    if (idle_rounds >= 5 && st != State::kRunning && st != State::kLoading)
                        return false;
                    return true;
                });
        };
    };
    svr.Get("/api/stream", make_stream_handler(false));
    svr.Get("/api/stream_raw", make_stream_handler(true));
}

void WebServer::Run(const std::string& host, int port) {
    httplib::Server svr;

    svr.set_payload_max_length(static_cast<size_t>(base_cfg_.max_upload_mb) * 1024 * 1024);

    const std::string& token = base_cfg_.auth_token;
    if (host == "0.0.0.0" && token.empty()) {
    }

    svr.set_mount_point("/", "./web");
    svr.set_mount_point("/data", "./data");

    RegisterFileRoutes(svr);
    RegisterPresetRoutes(svr);
    RegisterControlRoutes(svr);
    RegisterTaskRoutes(svr);
    RegisterStreamRoutes(svr);

    svr.new_task_queue = [] { return new httplib::ThreadPool(32); };

    if (!svr.bind_to_port(host, port)) {
        return;
    }
    svr.listen_after_bind();
}

}  // namespace omniface::server
