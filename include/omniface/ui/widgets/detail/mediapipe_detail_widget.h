#pragma once

#include "omniface/ui/widgets/detail/base_detail_widget.h"

#include <QLabel>
#include <QTextEdit>

#include <unordered_map>
#include <vector>

struct MediaPipeFrameData {
    int frame_number = 0;
    float pitch = 0, yaw = 0, roll = 0;
    int landmark_count = 0;
};

namespace omniface {
namespace ui {

class MediaPipeDetailWidget : public BaseDetailWidget {
    Q_OBJECT
public:
    explicit MediaPipeDetailWidget(QWidget* parent = nullptr);

    void LoadCsvData() override;
    void UpdateForFrame(int frameIndex) override;
    int TotalFrames() const override;

private:
    void SetupUI();
    QString FormatHeadPose(float pitch, float yaw, float roll);

    std::vector<MediaPipeFrameData> frames_;
    std::unordered_map<int, int> frame_to_index_;

    QTextEdit* text_display_ = nullptr;
    QLabel* frame_no_label_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
