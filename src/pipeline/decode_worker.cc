#include "omniface/pipeline/decode_worker.h"

#include <spdlog/spdlog.h>

namespace omniface {

DecodeWorker::DecodeWorker() = default;

DecodeWorker::~DecodeWorker() { Stop(); }

void DecodeWorker::Start(const std::string& video_path) {
    Stop();
    if (!cap_.open(video_path)) {
        spdlog::error("Failed to open video: {}", video_path);
        return;
    }
    spdlog::info("Video opened: {}", video_path);

    stop_requested_.store(false, std::memory_order_relaxed);
    frame_queue_.Reset();

    decode_thread_ = std::thread([this]() {
        cv::Mat frame;
        while (!stop_requested_.load(std::memory_order_relaxed)) {
            if (!cap_.read(frame)) break;
            if (frame.empty()) break;
            frame_queue_.Push(frame.clone());
        }
        spdlog::info("Decode thread finished");
        frame_queue_.Stop();
    });
}

void DecodeWorker::Stop() {
    stop_requested_.store(true, std::memory_order_relaxed);
    frame_queue_.Stop();
    if (decode_thread_.joinable()) {
        decode_thread_.join();
    }
    cap_.release();
}

bool DecodeWorker::PollFrame(cv::Mat& frame) {
    return frame_queue_.TryPop(frame);
}

bool DecodeWorker::IsStopped() const {
    return frame_queue_.IsStopped();
}

double DecodeWorker::GetFps() const {
    double fps = cap_.get(cv::CAP_PROP_FPS);
    return (fps > 0.0) ? fps : 24.0;
}

int DecodeWorker::GetFrameCount() const {
    return static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
}

}  // namespace omniface
