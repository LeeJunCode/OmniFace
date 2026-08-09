#pragma once

#include <string>
#include <vector>

namespace omniface::quality {

struct L2csQualityFrame {
    int frame_number = 0;
    float yaw_deg = 0, pitch_deg = 0;
    float yaw_maxp = 0, pitch_maxp = 0;
    float angular_velocity = 0;

    int score = 0;
    int score_conf = 0;   // /50
    int score_stab = 0;   // /50
    char grade = 'D';
};

struct L2csQualityReport {
    int total_frames = 0;
    int grade_s = 0, grade_a = 0, grade_b = 0, grade_c = 0, grade_d = 0;

    float mean_maxp = 0;
    float mean_angular_velocity = 0, max_angular_velocity = 0;

    std::vector<L2csQualityFrame> frames;
    std::string summary_text;

    float mean_score_conf = 0;
    float mean_score_stab = 0;
};

}  // namespace omniface::quality
