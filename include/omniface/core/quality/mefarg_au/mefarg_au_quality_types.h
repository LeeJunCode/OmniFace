#pragma once

#include <string>
#include <vector>

namespace omniface::quality {

struct MefargAuQualityFrame {
    int frame_number = 0;
    int active_au_count = 0;
    float mean_decision_margin = 0;
    int au_flips = 0;
    bool is_spike = false;

    int score = 0;
    int score_conf = 0;   // /50
    int score_stab = 0;   // /50
    char grade = 'D';
};

struct MefargAuQualityReport {
    int total_frames = 0;
    int grade_s = 0, grade_a = 0, grade_b = 0, grade_c = 0, grade_d = 0;

    float mean_decision_margin = 0;
    float overall_au_flip_rate = 0;
    int total_spike_frames = 0;

    std::vector<MefargAuQualityFrame> frames;
    std::string summary_text;

    float mean_score_conf = 0;
    float mean_score_stab = 0;
};

}  // namespace omniface::quality
