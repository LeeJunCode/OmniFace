#pragma once

#include <string>
#include <vector>

namespace omniface::clean {

enum class GradeCut { kS, kA, kB, kC, kNone };

struct CleanConfig {
    GradeCut min_grade = GradeCut::kB;
};

struct CleanResult {
    int total_original = 0;
    int total_clean = 0;
    int dropped_by_grade = 0;

    std::string summary_text;
};

}  // namespace omniface::clean
