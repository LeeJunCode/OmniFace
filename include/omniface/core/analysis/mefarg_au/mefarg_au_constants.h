#pragma once

#include <array>

namespace omniface::analysis::mefarg_au {

inline constexpr int kNumAuOutputs = 41;
inline constexpr int kNumMainAu = 27;
inline constexpr int kNumSubAu = 14;

inline constexpr std::array<const char*, kNumAuOutputs> kAuNames = {
    "AU1", "AU2", "AU4", "AU5", "AU6", "AU7", "AU9", "AU10",
    "AU11", "AU12", "AU13", "AU14", "AU15", "AU16", "AU17", "AU18",
    "AU19", "AU20", "AU22", "AU23", "AU24", "AU25", "AU26", "AU27",
    "AU32", "AU38", "AU39",
    "AUL1", "AUR1", "AUL2", "AUR2", "AUL4", "AUR4", "AUL6", "AUR6",
    "AUL10", "AUR10", "AUL12", "AUR12", "AUL14", "AUR14"
};

}  // namespace omniface::analysis::mefarg_au
