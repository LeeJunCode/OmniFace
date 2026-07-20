#pragma once

#include <deque>
#include <fstream>
#include <memory>
#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "omniface/core/analysis/au_analyzer.h"
#include "omniface/core/analysis/gaze_estimator.h"
#include "omniface/core/analysis/openface_analyzer.h"
#include "omniface/core/analysis/pose_estimator.h"
#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/face_matcher.h"
#include "omniface/core/recognition/face_recognizer.h"
#include "omniface/core/tracking/bytetrack_engine.h"
#include "omniface/pipeline/config.h"
#include "omniface/types.h"

namespace omniface::pipeline {

struct TrackBrief {
    int id = -1;
    float similarity = 0.0f;
    bool is_target = false;
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
};

struct FrameMetrics {
    int frame = 0;
    bool target_found = false;
    int target_track_id = -1;
    float similarity = 0.0f;
    bool pose_valid = false;
    float pitch = 0, yaw = 0, roll = 0;
    bool gaze_valid = false;
    float gaze_yaw = 0, gaze_pitch = 0;
    std::vector<std::pair<std::string, float>> aus;
    std::vector<TrackBrief> tracks;
};

class Pipeline {
public:
    explicit Pipeline(const AppConfig& cfg);

    bool Initialize();

    cv::Mat ProcessNextFrame();

    void Run();

    void Shutdown();

    bool IsOpen() const;
    int CurrentFrame() const {
        return frame_idx_;
    }
    int TotalFrames() const {
        return total_frames_;
    }
    double VideoFps() const {
        return video_fps_;
    }
    int TargetTrackId() const {
        return target_track_id_;
    }
    bool TargetFound() const {
        return target_track_id_ > 0;
    }

    const FrameMetrics& LastMetrics() const {
        return last_metrics_;
    }

    const cv::Mat& LastRawFrame() const {
        return last_raw_frame_;
    }

    const std::string& InitError() const {
        return init_error_;
    }

private:
    void PruneTrackCache(const std::vector<TrackedFace>& active_faces);

    void ProcessFrameCore(const cv::Mat& frame, std::vector<BoundingBox>& detections,
                          std::vector<TrackedFace>& tracked_faces,
                          std::vector<MatchResult>& match_results, int& best_match_idx,
                          float& best_match_sim);

    bool UpdateTargetState(const std::vector<TrackedFace>& tracked_faces,
                           const std::vector<MatchResult>& match_results, int best_match_idx,
                           float best_match_sim);

    cv::Mat AnalyzeWithOpenFace(const cv::Mat& frame, double timestamp);

    void AnalyzeWithOnnx(cv::Mat& frame);

    AppConfig cfg_;
    std::unique_ptr<detection::FaceDetector> detector_;
    std::unique_ptr<recognition::FaceRecognizer> recognizer_;
    std::unique_ptr<recognition::FaceMatcher> matcher_;
    std::unique_ptr<tracking::ByteTrackEngine> tracker_;

    std::unique_ptr<analysis::OpenFaceAnalyzer> openface_;
    std::unique_ptr<analysis::PoseEstimator> pose_estimator_;
    std::unique_ptr<analysis::GazeEstimator> gaze_estimator_;
    std::unique_ptr<analysis::AuAnalyzer> au_analyzer_;
    cv::Point3f nose_canonical_{};

    cv::VideoCapture capture_;
    cv::VideoWriter writer_;
    std::ofstream tracking_csv_;
    std::ofstream analysis_csv_;
    int openface_frame_count_ = 0;
    double video_fps_ = 30.0;
    int total_frames_ = 0;
    double frame_width_ = 0;
    double frame_height_ = 0;

    struct TrackInfo {
        bool is_target = false;
        float similarity = 0.0f;
    };
    std::unordered_map<int, TrackInfo> track_cache_;
    int target_track_id_ = -1;
    cv::Rect_<float> last_target_bbox_;
    bool openface_needs_reinit_ = true;
    int last_openface_track_id_ = -1;
    int frame_idx_ = 0;

    int last_target_verify_frame_ = 0;
    int target_verify_fails_ = 0;
    bool target_demote_ = false;

    float smoothed_gaze_yaw_ = 0.0f;
    float smoothed_gaze_pitch_ = 0.0f;
    bool gaze_smooth_init_ = false;
    std::deque<float> gaze_yaw_history_;
    std::deque<float> gaze_pitch_history_;

    std::deque<std::array<float, 41>> au_history_;

    FrameMetrics last_metrics_;
    std::string init_error_;
    cv::Mat last_raw_frame_;
};

}  // namespace omniface::pipeline
