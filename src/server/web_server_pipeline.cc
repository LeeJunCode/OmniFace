#include <chrono>
#include <filesystem>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <thread>

#include "omniface/pipeline/pipeline.h"
#include "omniface/server/web_server.h"

namespace omniface::server {

namespace fs = std::filesystem;
using namespace std::chrono_literals;

void WebServer::WorkerLoop(pipeline::AppConfig cfg, std::string task_id,
                           std::map<std::string, std::string> raw_params) {
    (void)raw_params;
    try {
        pipeline::Pipeline pl(cfg);
        if (!pl.Initialize()) {
            {
                std::lock_guard<std::mutex> lock(meta_mutex_);
                error_message_ = pl.InitError().empty() ? "UNKNOWN|初始化失败" : pl.InitError();
            }
            state_ = State::kError;
            WriteTaskJson(task_id, "error");
            return;
        }
        total_frames_ = pl.TotalFrames();
        video_fps_ = pl.VideoFps();
        state_ = State::kRunning;

        std::string task_dir = "data/output/" + task_id + "/";
        std::ofstream metrics_file(task_dir + "metrics.jsonl");

        std::vector<int> jpeg_params{cv::IMWRITE_JPEG_QUALITY, 80};
        std::vector<unsigned char> buf, raw_buf;

        auto win_start = std::chrono::steady_clock::now();
        int win_frames = 0;

        while (!stop_requested_) {
            if (paused_ && !step_once_.exchange(false)) {
                std::this_thread::sleep_for(50ms);
                continue;
            }

            cv::Mat frame = pl.ProcessNextFrame();
            if (frame.empty()) break;

            current_frame_ = pl.CurrentFrame();
            target_track_id_ = pl.TargetTrackId();

            std::string mjson = MetricsToJson(pl.LastMetrics());
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                metrics_json_.push_back(mjson);
            }
            if (metrics_file.is_open()) metrics_file << mjson << "\n";

            cv::imencode(".jpg", frame, buf, jpeg_params);
            if (!pl.LastRawFrame().empty())
                cv::imencode(".jpg", pl.LastRawFrame(), raw_buf, jpeg_params);
            {
                std::lock_guard<std::mutex> lock(frame_mutex_);
                latest_jpeg_ = buf;
                latest_raw_jpeg_ = raw_buf;
                frame_seq_++;
            }

            win_frames++;
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - win_start).count();
            if (elapsed >= 1.0) {
                fps_ = static_cast<float>(win_frames / elapsed);
                win_start = now;
                win_frames = 0;
            }
        }
        pl.Shutdown();
        state_ = State::kFinished;
        WriteTaskJson(task_id, stop_requested_ ? "stopped" : "finished");
    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> lock(meta_mutex_);
            error_message_ = std::string("RUNTIME|") + e.what();
        }
        state_ = State::kError;
        WriteTaskJson(task_id, "error");
    }
    fps_ = 0.0f;
}

void WebServer::WriteTaskJson(const std::string& task_id, const std::string& final_state) {
    int target_frames = 0;
    float sim_sum = 0, sim_max = 0;
    std::vector<std::pair<int, int>> intervals;
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        int start = -1, frame_no = 0;
        for (const auto& line : metrics_json_) {
            bool found = line.find("\"found\":true") != std::string::npos;
            if (found) {
                target_frames++;
                auto pos = line.find("\"sim\":");
                if (pos != std::string::npos) {
                    float s = std::strtof(line.c_str() + pos + 6, nullptr);
                    sim_sum += s;
                    sim_max = std::max(sim_max, s);
                }
                if (start < 0) start = frame_no;
            } else if (start >= 0) {
                intervals.emplace_back(start, frame_no - 1);
                start = -1;
            }
            frame_no++;
        }
        if (start >= 0) intervals.emplace_back(start, frame_no - 1);
    }

    std::lock_guard<std::mutex> lock(meta_mutex_);
    std::string task_dir = "data/output/" + task_id + "/";
    fs::create_directories(task_dir);
    std::ofstream f(task_dir + "task.json");
    f << "{\"id\":\"" << task_id << "\"" << ",\"state\":\"" << final_state << "\""
      << ",\"error\":\"" << JsonEscape(error_message_) << "\"" << ",\"backend\":\""
      << (active_cfg_.backend == pipeline::AnalysisBackend::kOnnx ? "onnx" : "openface") << "\""
      << ",\"reference\":\"" << JsonEscape(active_cfg_.reference_image) << "\"" << ",\"video\":\""
      << JsonEscape(active_cfg_.input_video) << "\"" << ",\"params\":{";
    bool first = true;
    for (const auto& [k, v] : active_params_) {
        if (!first) f << ",";
        f << "\"" << JsonEscape(k) << "\":\"" << JsonEscape(v) << "\"";
        first = false;
    }
    f << "},\"stats\":{\"frames\":" << current_frame_.load()
      << ",\"total_frames\":" << total_frames_.load() << ",\"fps\":" << video_fps_.load()
      << ",\"target_frames\":" << target_frames
      << ",\"avg_sim\":" << (target_frames > 0 ? sim_sum / target_frames : 0.0f)
      << ",\"max_sim\":" << sim_max << ",\"intervals\":[";
    for (size_t i = 0; i < intervals.size(); ++i) {
        if (i) f << ",";
        f << "[" << intervals[i].first << "," << intervals[i].second << "]";
    }
    f << "]},\"outputs\":{\"video\":\"" << JsonEscape(active_cfg_.output_video) << "\",\"csv\":\""
      << JsonEscape(active_cfg_.tracking_csv) << "\",\"openface\":\""
      << (active_cfg_.backend == pipeline::AnalysisBackend::kOpenFace
              ? JsonEscape(active_cfg_.openface.output_dir)
              : "")
      << "\",\"openface_csv\":\""
      << (active_cfg_.backend == pipeline::AnalysisBackend::kOpenFace
              ? JsonEscape(active_cfg_.openface.output_dir + active_cfg_.openface.output_name +
                           ".csv")
              : "")
      << "\",\"metrics\":\"data/output/" << task_id << "/metrics.jsonl\"" << ",\"onnx_csv\":\""
      << (active_cfg_.backend == pipeline::AnalysisBackend::kOnnx
              ? JsonEscape(active_cfg_.onnx_analysis_csv)
              : "")
      << "\"}}";
}

}  // namespace omniface::server
