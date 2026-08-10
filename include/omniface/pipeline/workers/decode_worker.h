#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>

#include "omniface/pipeline/framework/bounded_queue.h"

namespace omniface {

class DecodeWorker {
public:
    DecodeWorker();
    ~DecodeWorker();

    DecodeWorker(const DecodeWorker&) = delete;
    DecodeWorker& operator=(const DecodeWorker&) = delete;

    void Start(const std::string& video_path);
    void Stop();
    bool PollFrame(cv::Mat& frame);
    bool IsStopped() const;
    double GetFps() const;
    int GetFrameCount() const;

private:
    cv::VideoCapture cap_;
    BoundedQueue<cv::Mat> frame_queue_{4};
    std::thread decode_thread_;
    std::atomic<bool> stop_requested_{false};
};

}  // namespace omniface
