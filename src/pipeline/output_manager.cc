#include "omniface/pipeline/output_manager.h"

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

void OutputManager::Initialize(const cv::Size& frame_size, double fps,
                               detection::FaceDetector* detector) {
    if (initialized_ || config_.behavior.skip_analysis) return;

    CreateOutputDir();
    CopyOriginalVideo();
    CopyAndAnnotateReference(detector);

    namespace fs = std::filesystem;
    fs::path video_path(config_.video.input_path);
    auto video_stem = video_path.stem().string();
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

void OutputManager::CreateOutputDir() {
    namespace fs = std::filesystem;
    fs::path video_path(config_.video.input_path);
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
    fs::path video_path(config_.video.input_path);
    std::string orig_ext = video_path.extension().string();
    std::string orig_dst =
        (fs::path(output_dir_) / ("original" + orig_ext)).string();
    std::error_code ec;
    fs::copy_file(video_path, orig_dst,
                  fs::copy_options::overwrite_existing, ec);
    if (ec) spdlog::warn("Failed to copy original video to output: {}", ec.message());
}

void OutputManager::CopyAndAnnotateReference(detection::FaceDetector* detector) {
    if (config_.model.reference_image.empty()) return;
    namespace fs = std::filesystem;

    fs::path ref_path(config_.model.reference_image);
    if (!fs::exists(ref_path)) return;

    std::string ref_ext = ref_path.extension().string();
    std::string ref_dst =
        (fs::path(output_dir_) / ("reference" + ref_ext)).string();
    std::error_code ec;
    fs::copy_file(ref_path, ref_dst,
                  fs::copy_options::overwrite_existing, ec);
    if (ec) {
        spdlog::warn("Failed to copy reference image to output: {}", ec.message());
        return;
    }

    if (!detector) return;

    cv::Mat ref_img = cv::imread(ref_dst);
    if (ref_img.empty()) return;

    auto boxes = detector->Detect(ref_img);
    for (const auto& b : boxes) {
        cv::Rect r(static_cast<int>(b.x1), static_cast<int>(b.y1),
                   static_cast<int>(b.x2 - b.x1),
                   static_cast<int>(b.y2 - b.y1));
        cv::rectangle(ref_img, r, cv::Scalar(0, 220, 0), 4);
        for (int k = 0; k < 5; ++k) {
            cv::Point kp(static_cast<int>(b.keypoints[k * 2]),
                         static_cast<int>(b.keypoints[k * 2 + 1]));
            cv::circle(ref_img, kp, 3, cv::Scalar(0, 0, 255), -1);
        }
    }
    std::string detected_dst =
        (fs::path(output_dir_) / "reference_detected.png").string();
    cv::imwrite(detected_dst, ref_img);
}

}  // namespace omniface
