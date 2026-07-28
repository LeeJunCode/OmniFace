#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "omniface/types.h"

namespace omniface::ui {

struct AppState {
    std::string video_path;
    std::string reference_path;
    std::string last_output_dir;

    bool is_running = false;
    bool is_paused = false;
    bool step_requested = false;
    bool has_finished_once = false;
    bool recognition_only_ = false;

    int frame_count = 0;
    int total_frame_count = 0;
    double video_fps = 0.0;

    FaceEmbedding cached_reference_embedding;
    bool reference_cached = false;

    double current_fps = 0.0;
    int fps_frame_count = 0;
    std::chrono::steady_clock::time_point last_fps_time;

    int64_t total_faces_detected_ = 0;
    int64_t total_matches_ = 0;
    int64_t total_openface_analyses_ = 0;
    int64_t total_onnx_analyses_ = 0;
    std::chrono::steady_clock::time_point run_start_time_;
    std::chrono::steady_clock::time_point last_dashboard_time;
};

}  // namespace omniface::ui
