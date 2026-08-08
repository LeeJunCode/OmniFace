#pragma once

#include <memory>
#include <string>

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>

#include "omniface/core/detection/face_detector.h"
#include "omniface/pipeline/config.h"

namespace omniface {

class OutputManager {
public:
    explicit OutputManager(const PipelineConfig& config);

    void Initialize(const cv::Size& frame_size, double fps);
    void WriteFrame(const cv::Mat& frame);
    void Finalize();
    void SetVideoSource(const std::string& video_path);
    bool IsInitialized() const { return initialized_; }
    const std::string& GetOutputDir() const { return output_dir_; }
    const std::string& GetVideoSource() const { return video_source_path_; }
    const std::string& GetOutputVideoPath() const { return tracked_video_path_; }

private:
    void CreateOutputDir();
    void CopyOriginalVideo();

    PipelineConfig config_;
    std::string output_dir_;
    std::string video_source_path_;
    std::string tracked_video_path_;
    cv::VideoWriter writer_;
    bool initialized_ = false;
};

}  // namespace omniface
