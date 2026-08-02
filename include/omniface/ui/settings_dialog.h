#pragma once

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLineEdit>
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

    // 核心参数
    QDoubleSpinBox* match_threshold_ = nullptr;
    QDoubleSpinBox* detection_confidence_ = nullptr;
    QSpinBox* detection_threads_ = nullptr;
    QSpinBox* recognition_threads_ = nullptr;
    QComboBox* model_device_ = nullptr;

    // 高级参数
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

    // OpenFace 配置
    QComboBox* of_profile_ = nullptr;

    // 分析后端
    QComboBox* analysis_backend_ = nullptr;

    // ONNX 参数
    QSpinBox* onnx_threads_ = nullptr;
    QDoubleSpinBox* onnx_margin_ = nullptr;
    QDoubleSpinBox* onnx_score_threshold_ = nullptr;
    QComboBox* onnx_draw_landmarks_ = nullptr;
    QComboBox* onnx_draw_wireframe_ = nullptr;
    QComboBox* onnx_draw_normal_ = nullptr;
    QComboBox* onnx_device_ = nullptr;

    // 视线估计
    QComboBox* gaze_model_combo_ = nullptr;
    QComboBox* gaze_draw_ = nullptr;
    QComboBox* gaze_device_ = nullptr;
};

}  // namespace omniface::ui
