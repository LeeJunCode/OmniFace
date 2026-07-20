#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "omniface/core/detection/face_detector.h"
#include "omniface/pipeline/config.h"

namespace httplib {
struct Request;
struct Response;
class Server;
}  // namespace httplib
namespace omniface::pipeline {
struct FrameMetrics;
}

namespace omniface::server {

class WebServer {
public:
    explicit WebServer(pipeline::AppConfig base_cfg);
    ~WebServer();

    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;

    void Run(const std::string& host, int port);

private:
    enum class State { kIdle, kLoading, kRunning, kFinished, kError };

    void WorkerLoop(pipeline::AppConfig cfg, std::string task_id,
                    std::map<std::string, std::string> raw_params);
    void JoinWorker();
    void WriteTaskJson(const std::string& task_id, const std::string& final_state);
    std::string StatusJson() const;

    bool CheckAuth(const httplib::Request& req, httplib::Response& res) const;
    void RegisterFileRoutes(httplib::Server& svr);
    void RegisterPresetRoutes(httplib::Server& svr);
    void RegisterControlRoutes(httplib::Server& svr);
    void RegisterTaskRoutes(httplib::Server& svr);
    void RegisterStreamRoutes(httplib::Server& svr);

    pipeline::AppConfig base_cfg_;

    std::thread worker_;
    std::atomic<State> state_{State::kIdle};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> step_once_{false};
    std::atomic<int> current_frame_{0};
    std::atomic<int> total_frames_{0};
    std::atomic<double> video_fps_{0.0};
    std::atomic<int> target_track_id_{-1};
    std::atomic<float> fps_{0.0f};

    mutable std::mutex meta_mutex_;
    std::string task_id_;
    std::string error_message_;
    pipeline::AppConfig active_cfg_;
    std::map<std::string, std::string> active_params_;

    mutable std::mutex metrics_mutex_;
    std::vector<std::string> metrics_json_;

    mutable std::mutex frame_mutex_;
    std::vector<unsigned char> latest_jpeg_;
    uint64_t frame_seq_ = 0;
    std::vector<unsigned char> latest_raw_jpeg_;

    std::mutex detector_mutex_;
    std::unique_ptr<detection::FaceDetector> probe_detector_;
};

std::string JsonEscape(const std::string& s);
bool PathInside(const std::string& path, const std::string& allowed_prefix);
bool ValidTaskId(const std::string& id);
std::string MakeTaskId();
std::string ListFilesJson(const std::string& dir, const std::vector<std::string>& exts);
std::string MetricsToJson(const pipeline::FrameMetrics& m);
const char* StateName(int s);
void ApplyParams(const httplib::Request& req, pipeline::AppConfig& cfg);

}  // namespace omniface::server
