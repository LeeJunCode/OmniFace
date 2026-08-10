#include "omniface/pipeline/workers/output_manager.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

namespace omniface {

OutputManager::OutputManager(const PipelineConfig& config)
    : config_(config) {}

void OutputManager::Initialize(const cv::Size& frame_size, double fps) {
    if (initialized_ || config_.behavior.skip_analysis) return;

    CreateOutputDir();
    CopyOriginalVideo();

    namespace fs = std::filesystem;
    fs::path video_path(video_source_path_);
    auto video_stem = video_path.stem().string();

    // MJPG in AVI — OpenCV's built-in codec, guaranteed available without FFmpeg.
    tracked_video_path_ =
        (fs::path(output_dir_) / (video_stem + ".avi")).string();
    writer_.open(tracked_video_path_,
                 cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                 fps, frame_size, true);

    if (!writer_.isOpened())
        spdlog::warn("Failed to open tracked video: {}", tracked_video_path_);
    else
        spdlog::info("Tracked video output: {}", tracked_video_path_);

    initialized_ = true;
}

void OutputManager::WriteFrame(const cv::Mat& frame) {
    if (writer_.isOpened())
        writer_.write(frame);
}

void OutputManager::Finalize() {
    if (writer_.isOpened()) {
        writer_.release();
        spdlog::info("Tracked video finalized: {}", tracked_video_path_);
    }
}

void OutputManager::CreateOutputDir() {
    namespace fs = std::filesystem;
    fs::path video_path(video_source_path_);
    auto video_stem = video_path.stem().string();

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &t);
    char ts[16];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm_buf);
    fs::path base = config_.output_base_dir.empty()
        ? fs::path("data/output") : fs::path(config_.output_base_dir);
    output_dir_ = (base / (video_stem + "_" + std::string(ts))).string();
    fs::create_directories(output_dir_);
}

void OutputManager::CopyOriginalVideo() {
    namespace fs = std::filesystem;
    fs::path video_path(video_source_path_);
    std::string orig_ext = video_path.extension().string();
    std::string orig_dst =
        (fs::path(output_dir_) / ("original" + orig_ext)).string();
    std::error_code ec;
    fs::copy_file(video_path, orig_dst,
                  fs::copy_options::overwrite_existing, ec);
    if (ec) spdlog::warn("Failed to copy original video to output: {}", ec.message());
}

void OutputManager::SetVideoSource(const std::string& video_path) {
    video_source_path_ = video_path;
}

}  // namespace omniface
