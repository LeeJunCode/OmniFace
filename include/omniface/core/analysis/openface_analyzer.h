#pragma once

#include <memory>
#include <opencv2/core.hpp>
#include <string>
#include <utility>
#include <vector>

namespace omniface::analysis {

class OpenFaceAnalyzer {
public:
    struct FrameResult {
        bool valid = false;
        float pitch = 0, yaw = 0, roll = 0;
        float gaze_yaw = 0, gaze_pitch = 0;
        std::vector<std::pair<std::string, float>> aus;
    };

    explicit OpenFaceAnalyzer(const std::string& model_dir);
    ~OpenFaceAnalyzer();

    OpenFaceAnalyzer(const OpenFaceAnalyzer&) = delete;
    OpenFaceAnalyzer& operator=(const OpenFaceAnalyzer&) = delete;

    void StartSequence(const std::string& output_name, const std::string& output_dir, double fx,
                       double fy, double cx, double cy, double fps = 30.0);
    void AnalyzeVideoFrame(const cv::Mat& bgr, double timestamp);
    void AnalyzeVideoFrame(const cv::Mat& bgr, double timestamp,
                           const cv::Rect_<float>& target_bbox);
    void EndSequence();

    bool IsQuitRequested() const;
    cv::Mat GetVisualization() const;
    void SetVisualization(bool track_pose_gaze, bool aus, bool hog, bool align);

    const FrameResult& LastResult() const;

    void SetFacePreference(double norm_x, double norm_y);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::analysis
