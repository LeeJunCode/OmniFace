#pragma once

#include <string>
#include <vector>

namespace omniface::quality {

struct OpenFaceQualityFrame {
    int frame_number = 0;
    float confidence = 0.0f;
    bool success = false;
    float quality_score = 0.0f;
    int visible_count = 68;

    float pitch_deg = 0, yaw_deg = 0, roll_deg = 0;
    bool pose_anomaly = false;
    float angular_velocity = 0;

    float gaze_angle_diff = 0;
    bool gaze_failed = false;

    int active_au_count = 0;
    std::vector<int> active_au_ids;
    bool facs_violation = false;

    int score = 0;  // 0-100 weighted score
    char grade = 'D';  // S/A/B/C/D
    float decision_margin = 0;
};

struct OpenFaceQualityReport {
    int total_frames = 0;
    int grade_s = 0, grade_a = 0, grade_b = 0, grade_c = 0, grade_d = 0;

    float mean_confidence = 0, std_confidence = 0;
    float mean_quality_score = 0;
    float mean_visible_count = 0;
    float mean_gaze_diff = 0, max_gaze_diff = 0;
    float mean_angular_velocity = 0, max_angular_velocity = 0;

    float overall_au_flip_rate = 0;
    float gaze_fail_rate = 0;
    int frames_pose_anomaly = 0;
    int frames_facs_violation = 0;

    std::vector<OpenFaceQualityFrame> frames;
    std::vector<float> confidence_histogram;
    std::vector<int> au_flip_counts;
    std::vector<float> au_intensity_means;

    std::string summary_text;

    bool eigenvalues_valid = false;
    std::vector<float> pdm_eigenvalues;

    // Per-dimension mean contribution scores (aligned with AssignFrameGrades weights)
    float mean_score_confidence = 0;  // /25
    float mean_score_visible = 0;     // /15
    float mean_score_pose = 0;        // /15

    float mean_score_gaze = 0;        // /15
    float mean_score_angular = 0;     // /10
    float mean_score_au = 0;          // /10
};

}  // namespace omniface::quality
