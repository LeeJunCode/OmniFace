#pragma once

#include "omniface/ui/widgets/detail/base_detail_widget.h"

#include <QLabel>
#include <QTextEdit>

#include <unordered_map>
#include <vector>

struct L2csFrameData {
    int frame_number = 0;
    float yaw = 0, pitch = 0;
    float yaw_maxp = 0, pitch_maxp = 0;
    float gaze_x = 0, gaze_y = 0, gaze_z = 0;
};

namespace omniface {
namespace ui {

class L2csDetailWidget : public BaseDetailWidget {
    Q_OBJECT
public:
    explicit L2csDetailWidget(QWidget* parent = nullptr);

    void LoadCsvData() override;
    void UpdateForFrame(int frameIndex) override;
    int TotalFrames() const override;
    void SetFrameGrades(const std::unordered_map<int, char>& grades);

private:
    void SetupUI();
    QString FormatGaze(float yaw, float pitch);

    std::vector<L2csFrameData> frames_;
    std::unordered_map<int, int> frame_to_index_;
    std::unordered_map<int, char> frame_grades_;

    QTextEdit* text_display_ = nullptr;
    QLabel* frame_no_label_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
