#pragma once

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>

#include "omniface/pipeline/config.h"

namespace omniface::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const PipelineConfig& current, QWidget* parent = nullptr);

    PipelineConfig getConfig() const;

private:
    void setupUI();
    void saveTo(PipelineConfig& cfg) const;
    PipelineConfig config_;

    // Core
    QDoubleSpinBox* match_threshold_ = nullptr;
    QDoubleSpinBox* detection_confidence_ = nullptr;
    QSpinBox* detection_threads_ = nullptr;
    QSpinBox* recognition_threads_ = nullptr;

    // Advanced
    QDoubleSpinBox* detection_nms_ = nullptr;
    QDoubleSpinBox* min_face_area_ = nullptr;
    QDoubleSpinBox* recognition_confidence_threshold_ = nullptr;
    QDoubleSpinBox* recognition_iou_ = nullptr;
    QDoubleSpinBox* tracking_high_ = nullptr;
    QDoubleSpinBox* tracking_match_ = nullptr;
    QDoubleSpinBox* tracking_match_second_ = nullptr;
    QDoubleSpinBox* tracking_new_ = nullptr;
    QDoubleSpinBox* ema_alpha_ = nullptr;
    QSpinBox* max_frame_width_ = nullptr;
    QComboBox* log_level_ = nullptr;

    // OpenFace
    QComboBox* of_profile_ = nullptr;
};

}  // namespace omniface::ui
