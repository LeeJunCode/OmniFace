#pragma once

#include <string>
#include <vector>

namespace omniface::quality {

struct MediaPipeQualityFrame {
    int frame_number = 0;
    float reproj_rms = 0.0f;
    float face_size = 0.0f;    // landmark bounding-box diagonal (px)

    float pitch_deg = 0, yaw_deg = 0, roll_deg = 0;
    bool pose_anomaly = false;
    float angular_velocity = 0;

    int score = 0;        // 0-100: geometry (50) + pose (50)
    int score_geo = 0;    // /50
    int score_pose = 0;   // /50
    char grade = 'D';
};

struct MediaPipeQualityReport {
    int total_frames = 0;
    int grade_s = 0, grade_a = 0, grade_b = 0, grade_c = 0, grade_d = 0;

    float mean_reproj_rms = 0, max_reproj_rms = 0;
    float mean_nre = 0, max_nre = 0;          // normalized reprojection error (%)
    float mean_angular_velocity = 0, max_angular_velocity = 0;

    int frames_pose_anomaly = 0;

    std::vector<MediaPipeQualityFrame> frames;
    std::vector<float> nre_histogram;    // NRE bins (%)

    std::string summary_text;

    float mean_score_geo = 0;    // Geometry /50
    float mean_score_pose = 0;   // Pose /50
};

}  // namespace omniface::quality
