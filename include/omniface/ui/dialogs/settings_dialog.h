#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QSpinBox>
#include <QStackedWidget>

#include "omniface/pipeline/config.h"

namespace omniface {
namespace ui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    const PipelineConfig& GetConfig() const { return config_; }
    void SetConfig(const PipelineConfig& cfg);

signals:
    void ConfigChanged(const PipelineConfig& config);

private slots:
    void OnSectionChanged(int row);
    void OnBackendChanged(int index);
    void OnDeviceChanged(int index);
    void OnResetAll();
    void OnAccepted();

private:
    void SetupUI();
    void LoadToUI();
    void SaveFromUI();

    QWidget* CreateGeneralPanel();
    QWidget* CreateBackendPanel();
    QWidget* CreateBackendParamsPanel(AnalysisBackend be);
    QWidget* CreateAdvancedPanel();
    QWidget* CreateDeviceCombo();

    PipelineConfig config_;

    QListWidget* sidebar_ = nullptr;
    QStackedWidget* panels_ = nullptr;

    // ── 常用参数 ────────────────────────────────────────────
    QDoubleSpinBox* match_threshold_spin_ = nullptr;
    QComboBox* device_combo_ = nullptr;
    QComboBox* log_level_combo_ = nullptr;

    // ── 分析后端 ────────────────────────────────────────────
    QComboBox* backend_combo_ = nullptr;
    QStackedWidget* backend_params_stack_ = nullptr;

    // OpenFace
    QComboBox* of_profile_combo_ = nullptr;
    QDoubleSpinBox* of_sigma_spin_ = nullptr;
    QDoubleSpinBox* of_reg_spin_ = nullptr;
    QDoubleSpinBox* of_weight_spin_ = nullptr;
    QSpinBox* of_optimise_spin_ = nullptr;
    QCheckBox* of_template_cb_ = nullptr;
    QDoubleSpinBox* of_template_scale_spin_ = nullptr;
    QCheckBox* of_validate_cb_ = nullptr;
    QDoubleSpinBox* of_boundary_spin_ = nullptr;
    QCheckBox* of_refine_hier_cb_ = nullptr;
    QCheckBox* of_refine_param_cb_ = nullptr;
    QCheckBox* of_multiview_cb_ = nullptr;
    QSpinBox* of_reinit_spin_ = nullptr;
    QCheckBox* of_limit_pose_cb_ = nullptr;

    // MediaPipe
    QCheckBox* mp_enable_pose_cb_ = nullptr;
    QDoubleSpinBox* mp_margin_spin_ = nullptr;
    QDoubleSpinBox* mp_score_spin_ = nullptr;
    QCheckBox* mp_draw_landmarks_cb_ = nullptr;
    QCheckBox* mp_draw_wireframe_cb_ = nullptr;
    QCheckBox* mp_draw_normal_cb_ = nullptr;
    QComboBox* mp_device_combo_ = nullptr;

    // L2CS-Net
    QComboBox* l2cs_model_combo_ = nullptr;
    QCheckBox* l2cs_draw_gaze_cb_ = nullptr;
    QComboBox* l2cs_device_combo_ = nullptr;

    // MEFARG-AU
    QComboBox* mefarg_model_combo_ = nullptr;
    QDoubleSpinBox* mefarg_threshold_spin_ = nullptr;
    QCheckBox* mefarg_draw_au_cb_ = nullptr;
    QComboBox* mefarg_device_combo_ = nullptr;

    // MediaPipe
    QComboBox* mp_model_combo_ = nullptr;

    // ── 高级参数 ────────────────────────────────────────────
    // Detection
    QSpinBox* det_input_w_spin_ = nullptr;
    QSpinBox* det_input_h_spin_ = nullptr;
    QDoubleSpinBox* min_face_area_spin_ = nullptr;
    QDoubleSpinBox* det_conf_spin_ = nullptr;
    QDoubleSpinBox* det_nms_spin_ = nullptr;

    // Tracking
    QDoubleSpinBox* track_high_spin_ = nullptr;
    QDoubleSpinBox* track_match_spin_ = nullptr;
    QDoubleSpinBox* track_match2_spin_ = nullptr;
    QDoubleSpinBox* track_new_spin_ = nullptr;

    // Recognition
    QDoubleSpinBox* rec_iou_spin_ = nullptr;

    // Behavior
    QDoubleSpinBox* ema_alpha_spin_ = nullptr;
    QSpinBox* max_frame_width_spin_ = nullptr;
    QSpinBox* target_recheck_spin_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
