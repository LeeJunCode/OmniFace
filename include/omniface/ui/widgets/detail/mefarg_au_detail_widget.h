#pragma once

#include "omniface/ui/widgets/detail/base_detail_widget.h"

#include <QLabel>
#include <QTextEdit>

#include <array>
#include <string>
#include <unordered_map>

namespace omniface {
namespace ui {

class MefargAuDetailWidget : public BaseDetailWidget {
    Q_OBJECT
public:
    explicit MefargAuDetailWidget(QWidget* parent = nullptr);

    void LoadCsvData() override;
    void UpdateForFrame(int frameIndex) override;
    int TotalFrames() const override;
    void SetFrameGrades(const std::unordered_map<int, char>& grades);

private:
    void SetupUI();
    QString FormatAU(int au_index, float probability);

    static constexpr int kNumAU = 41;
    static const std::array<std::string, kNumAU> au_names_;

    // Per-frame: 41 AU probabilities
    std::vector<std::array<float, kNumAU>> frame_au_probs_;
    std::vector<int> frame_numbers_;
    std::unordered_map<int, int> frame_to_index_;
    std::unordered_map<int, char> frame_grades_;

    QTextEdit* text_display_ = nullptr;
    QLabel* frame_no_label_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
