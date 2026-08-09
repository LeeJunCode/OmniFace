#pragma once

#include "omniface/ui/widgets/detail/base_detail_widget.h"

#include <QLabel>
#include <QTextEdit>

#include <unordered_map>
#include <vector>

struct OpenFaceFrameData {
    float confidence = 0;
    float pose_rx = 0, pose_ry = 0, pose_rz = 0;  // pitch, yaw, roll (degrees)
    float pose_tx = 0, pose_ty = 0, pose_tz = 0;
    float gaze_0_x = 0, gaze_0_y = 0, gaze_0_z = 0;  // left eye 3D gaze
    float gaze_1_x = 0, gaze_1_y = 0, gaze_1_z = 0;  // right eye 3D gaze
    float gaze_angle_x = 0, gaze_angle_y = 0;
    float detection_certainty = 0;
    bool valid = false;
    int frame_number = 0;
    std::vector<std::pair<int, float>> activated_aus;  // AU number → intensity
};

namespace omniface {
namespace ui {

class OpenFaceDetailWidget : public BaseDetailWidget {
    Q_OBJECT
public:
    explicit OpenFaceDetailWidget(QWidget* parent = nullptr);

    void LoadCsvData() override;
    void UpdateForFrame(int frameIndex) override;
    int TotalFrames() const override;
    void SetFrameGrades(const std::unordered_map<int, char>& grades);

private:
    void SetupUI();

    // Parse header row to find column indices
    std::unordered_map<std::string, int> BuildColumnMap(const QStringList& header_cols);
    OpenFaceFrameData ParseRow(const QStringList& cols,
                               const std::unordered_map<std::string, int>& col_map);

    std::vector<OpenFaceFrameData> frames_;
    std::unordered_map<int, int> frame_to_index_;
    std::unordered_map<int, char> frame_grades_;

    QLabel* face_image_label_ = nullptr;
    QTextEdit* text_display_ = nullptr;
    QLabel* frame_no_label_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
