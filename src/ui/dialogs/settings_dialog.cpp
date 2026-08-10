#include "omniface/ui/dialogs/settings_dialog.h"
#include "omniface/ui/theme.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

#include "omniface/core/inference/onnx_session.h"

namespace omniface {
namespace ui {

namespace {

QWidget* WrapInScroll(QWidget* w) {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setWidget(w);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}

QDoubleSpinBox* MakeDoubleSpin(QWidget* parent, double min, double max, double step, int decimals = 3) {
    auto& tm = ThemeManager::Instance();
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 6px; color: %3; }"
        "QDoubleSpinBox:focus { border-color: %4; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    return spin;
}

QSpinBox* MakeSpinBox(QWidget* parent, int min, int max) {
    auto& tm = ThemeManager::Instance();
    auto* spin = new QSpinBox(parent);
    spin->setRange(min, max);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setStyleSheet(QStringLiteral(
        "QSpinBox { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 6px; color: %3; }"
        "QSpinBox:focus { border-color: %4; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    return spin;
}

QComboBox* MakeCombo(QWidget* parent, const QStringList& items) {
    auto& tm = ThemeManager::Instance();
    auto* combo = new QComboBox(parent);
    combo->addItems(items);
    combo->setStyleSheet(QStringLiteral(
        "QComboBox { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 8px; color: %3; }"
        "QComboBox:focus { border-color: %4; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: %5; color: %6;"
        "  selection-background-color: %7; border: 1px solid %8; }")
        .arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary(),
             tm.SurfaceBg(), tm.TextPrimary(), tm.SelectedBg(), tm.Border()));
    return combo;
}

QCheckBox* MakeCheckBox(const QString& text, QWidget* parent) {
    auto& tm = ThemeManager::Instance();
    auto* cb = new QCheckBox(text, parent);
    cb->setStyleSheet(QStringLiteral(
        "QCheckBox { color: %1; spacing: 6px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }").arg(tm.TextPrimary()));
    return cb;
}

QString GroupBoxStyle() {
    auto& tm = ThemeManager::Instance();
    return QStringLiteral(
        "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
        "  border-radius: 4px; margin-top: 12px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }")
        .arg(tm.TextPrimary(), tm.Border());
}

}  // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent), config_() {
    auto& tm = ThemeManager::Instance();
    setWindowTitle(QStringLiteral("设置"));
    resize(800, 600);
    setMinimumSize(700, 500);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; }"
        "QLabel { color: %2; }").arg(tm.SurfaceBg(), tm.TextPrimary()));

    SetupUI();
    LoadToUI();
}

void SettingsDialog::SetConfig(const PipelineConfig& cfg) {
    config_ = cfg;
    LoadToUI();
}

// ═══════════════════════════════════════════════════════════════
// UI Setup
// ═══════════════════════════════════════════════════════════════

void SettingsDialog::SetupUI() {
    auto& tm = ThemeManager::Instance();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);

    // ── Sidebar ──────────────────────────────────────────────

    sidebar_ = new QListWidget(this);
    sidebar_->setFixedWidth(140);
    sidebar_->addItem(QStringLiteral("常用参数"));
    sidebar_->addItem(QStringLiteral("分析后端"));
    sidebar_->addItem(QStringLiteral("高级参数"));
    sidebar_->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; border: none; outline: none; }"
        "QListWidget::item { color: %2; padding: 10px 14px; }"
        "QListWidget::item:selected { color: %3; background: %4; }"
        "QListWidget::item:hover { color: %3; }").arg(tm.DeepBg(), tm.TextMuted(), tm.TextPrimary(), tm.SelectedBg()));
    connect(sidebar_, &QListWidget::currentRowChanged, this, &SettingsDialog::OnSectionChanged);
    splitter->addWidget(sidebar_);

    // ── Panels ───────────────────────────────────────────────

    panels_ = new QStackedWidget(this);
    panels_->addWidget(WrapInScroll(CreateGeneralPanel()));
    panels_->addWidget(WrapInScroll(CreateBackendPanel()));
    panels_->addWidget(WrapInScroll(CreateAdvancedPanel()));
    splitter->addWidget(panels_);

    root->addWidget(splitter, 1);

    // ── Bottom bar ───────────────────────────────────────────

    auto* bottom = new QHBoxLayout();
    bottom->setContentsMargins(12, 8, 12, 8);
    bottom->setSpacing(8);

    auto makeBtn = [this, &tm](const QString& text, bool primary = false) {
        auto* btn = new QPushButton(text, this);
        btn->setStyleSheet(primary ?
            QStringLiteral("QPushButton { background: %1; color: %2; border: none;"
                "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
                "QPushButton:hover { background: %3; }")
                .arg(tm.Primary(), tm.White(), tm.PrimaryHover()) :
            QStringLiteral("QPushButton { background: %1; color: %2; border: none;"
                "  border-radius: 4px; padding: 6px 16px; }"
                "QPushButton:hover { background: %3; }")
                .arg("transparent", tm.TextPrimary(), tm.HoverBg()));
        return btn;
    };

    auto* resetAllBtn = makeBtn(QStringLiteral("恢复默认"));
    connect(resetAllBtn, &QPushButton::clicked, this, &SettingsDialog::OnResetAll);
    bottom->addWidget(resetAllBtn);

    bottom->addStretch();

    auto* cancelBtn = makeBtn(QStringLiteral("取消"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottom->addWidget(cancelBtn);

    auto* okBtn = makeBtn(QStringLiteral("确定"), true);
    connect(okBtn, &QPushButton::clicked, this, &SettingsDialog::OnAccepted);
    bottom->addWidget(okBtn);

    root->addLayout(bottom);

    sidebar_->setCurrentRow(0);
}

QWidget* SettingsDialog::CreateDeviceCombo() {
    return MakeCombo(this, {QStringLiteral("CPU"), QStringLiteral("DirectML"),
                            QStringLiteral("CUDA"), QStringLiteral("TensorRT")});
}

// ═══════════════════════════════════════════════════════════════
// 常用参数
// ═══════════════════════════════════════════════════════════════

QWidget* SettingsDialog::CreateGeneralPanel() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* group = new QGroupBox(QStringLiteral("常用参数"), page);
    group->setStyleSheet(GroupBoxStyle());
    auto* layout = new QFormLayout(group);
    layout->setSpacing(8);

    match_threshold_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
    match_threshold_spin_->setValue(0.30);
    layout->addRow(QStringLiteral("匹配相似度阈值:"), match_threshold_spin_);

    device_combo_ = qobject_cast<QComboBox*>(CreateDeviceCombo());
    connect(device_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::OnDeviceChanged);
    layout->addRow(QStringLiteral("推理加速:"), device_combo_);

    log_level_combo_ = MakeCombo(this, {QStringLiteral("debug"),
                                        QStringLiteral("info"),
                                        QStringLiteral("off")});
    log_level_combo_->setCurrentText(QStringLiteral("debug"));
    layout->addRow(QStringLiteral("日志级别:"), log_level_combo_);

    form->addRow(group);
    return page;
}

// ═══════════════════════════════════════════════════════════════
// 分析后端
// ═══════════════════════════════════════════════════════════════

QWidget* SettingsDialog::CreateBackendPanel() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(10);

    backend_combo_ = MakeCombo(this, {QStringLiteral("OpenFace"), QStringLiteral("MediaPipe"),
                                      QStringLiteral("L2CS-Net"), QStringLiteral("MEFARG-AU"),
                                      QStringLiteral("None")});
    connect(backend_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::OnBackendChanged);
    form->addRow(QStringLiteral("分析后端:"), backend_combo_);

    backend_params_stack_ = new QStackedWidget(this);
    backend_params_stack_->addWidget(CreateBackendParamsPanel(AnalysisBackend::kOpenFace));
    backend_params_stack_->addWidget(CreateBackendParamsPanel(AnalysisBackend::kMediaPipe));
    backend_params_stack_->addWidget(CreateBackendParamsPanel(AnalysisBackend::kL2cs));
    backend_params_stack_->addWidget(CreateBackendParamsPanel(AnalysisBackend::kMefargAu));
    backend_params_stack_->addWidget(CreateBackendParamsPanel(AnalysisBackend::kNone));
    form->addRow(backend_params_stack_);

    return page;
}

QWidget* SettingsDialog::CreateBackendParamsPanel(AnalysisBackend be) {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    form->setContentsMargins(0, 8, 0, 0);
    form->setSpacing(6);

    switch (be) {
    case AnalysisBackend::kOpenFace: {
        of_profile_combo_ = MakeCombo(this, {QStringLiteral("standard"), QStringLiteral("max_quality")});
        form->addRow(QStringLiteral("模式:"), of_profile_combo_);

        of_sigma_spin_ = MakeDoubleSpin(this, -1.0, 10.0, 0.1, 1);
        of_sigma_spin_->setSpecialValueText(QStringLiteral("使用默认"));
        of_sigma_spin_->setValue(-1.0);
        form->addRow(QStringLiteral("Sigma:"), of_sigma_spin_);

        of_reg_spin_ = MakeDoubleSpin(this, -1.0, 100.0, 1.0, 1);
        of_reg_spin_->setSpecialValueText(QStringLiteral("使用默认"));
        of_reg_spin_->setValue(-1.0);
        form->addRow(QStringLiteral("Reg Factor:"), of_reg_spin_);

        of_weight_spin_ = MakeDoubleSpin(this, -1.0, 10.0, 0.1, 1);
        of_weight_spin_->setSpecialValueText(QStringLiteral("使用默认"));
        of_weight_spin_->setValue(-1.0);
        form->addRow(QStringLiteral("Weight Factor:"), of_weight_spin_);

        of_optimise_spin_ = MakeSpinBox(this, -1, 200);
        of_optimise_spin_->setSpecialValueText(QStringLiteral("使用默认"));
        of_optimise_spin_->setValue(-1);
        form->addRow(QStringLiteral("优化迭代次数:"), of_optimise_spin_);

        of_template_cb_ = MakeCheckBox(QStringLiteral("使用人脸模板"), page);
        form->addRow({}, of_template_cb_);

        of_template_scale_spin_ = MakeDoubleSpin(this, -1.0, 2.0, 0.05, 2);
        of_template_scale_spin_->setSpecialValueText(QStringLiteral("使用默认"));
        of_template_scale_spin_->setValue(-1.0);
        form->addRow(QStringLiteral("模板缩放:"), of_template_scale_spin_);

        of_validate_cb_ = MakeCheckBox(QStringLiteral("验证检测结果"), page);
        form->addRow({}, of_validate_cb_);

        of_boundary_spin_ = MakeDoubleSpin(this, -1.0, 1.0, 0.05, 2);
        of_boundary_spin_->setSpecialValueText(QStringLiteral("使用默认"));
        of_boundary_spin_->setValue(-1.0);
        form->addRow(QStringLiteral("验证边界:"), of_boundary_spin_);

        of_refine_hier_cb_ = MakeCheckBox(QStringLiteral("分层细化"), page);
        form->addRow({}, of_refine_hier_cb_);

        of_refine_param_cb_ = MakeCheckBox(QStringLiteral("参数细化"), page);
        form->addRow({}, of_refine_param_cb_);

        of_multiview_cb_ = MakeCheckBox(QStringLiteral("多视角首帧"), page);
        form->addRow({}, of_multiview_cb_);

        of_reinit_spin_ = MakeSpinBox(this, -1, 1000);
        of_reinit_spin_->setSpecialValueText(QStringLiteral("不重新初始化"));
        of_reinit_spin_->setValue(-1);
        form->addRow(QStringLiteral("重新初始化间隔:"), of_reinit_spin_);

        of_limit_pose_cb_ = MakeCheckBox(QStringLiteral("限制姿态范围"), page);
        form->addRow({}, of_limit_pose_cb_);
        break;
    }
    case AnalysisBackend::kMediaPipe: {
        mp_model_combo_ = MakeCombo(this, {QStringLiteral("Face Mesh (Nx3x192x192)")});
        mp_model_combo_->setItemData(0, QStringLiteral("models/mediapipe/face_mesh_Nx3x192x192_post.onnx"));
        form->addRow(QStringLiteral("模型:"), mp_model_combo_);

        mp_enable_pose_cb_ = MakeCheckBox(QStringLiteral("启用头部姿态估计"), page);
        mp_enable_pose_cb_->setChecked(true);
        form->addRow({}, mp_enable_pose_cb_);

        mp_margin_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
        mp_margin_spin_->setValue(0.25);
        form->addRow(QStringLiteral("Margin:"), mp_margin_spin_);

        mp_score_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
        mp_score_spin_->setValue(0.95);
        form->addRow(QStringLiteral("Score Threshold:"), mp_score_spin_);

        mp_draw_landmarks_cb_ = MakeCheckBox(QStringLiteral("绘制地标点"), page);
        mp_draw_landmarks_cb_->setChecked(true);
        form->addRow({}, mp_draw_landmarks_cb_);

        mp_draw_wireframe_cb_ = MakeCheckBox(QStringLiteral("绘制线框"), page);
        mp_draw_wireframe_cb_->setChecked(true);
        form->addRow({}, mp_draw_wireframe_cb_);

        mp_draw_normal_cb_ = MakeCheckBox(QStringLiteral("绘制法线"), page);
        mp_draw_normal_cb_->setChecked(true);
        form->addRow({}, mp_draw_normal_cb_);

        mp_device_combo_ = qobject_cast<QComboBox*>(CreateDeviceCombo());
        connect(mp_device_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SettingsDialog::OnDeviceChanged);
        form->addRow(QStringLiteral("推理设备:"), mp_device_combo_);
        break;
    }
    case AnalysisBackend::kL2cs: {
        l2cs_model_combo_ = MakeCombo(this, {
            QStringLiteral("MobileNetV2 (轻量)"),
            QStringLiteral("ResNet50 (高精度)"),
            QStringLiteral("MobileOne-S0 (均衡)")});
        l2cs_model_combo_->setItemData(0, QStringLiteral("models/l2cs-net/mobilenetv2_gaze.onnx"));
        l2cs_model_combo_->setItemData(1, QStringLiteral("models/l2cs-net/resnet50_gaze.onnx"));
        l2cs_model_combo_->setItemData(2, QStringLiteral("models/l2cs-net/mobileone_s0_gaze.onnx"));
        form->addRow(QStringLiteral("模型:"), l2cs_model_combo_);

        l2cs_draw_gaze_cb_ = MakeCheckBox(QStringLiteral("绘制视线方向"), page);
        l2cs_draw_gaze_cb_->setChecked(true);
        form->addRow({}, l2cs_draw_gaze_cb_);

        l2cs_device_combo_ = qobject_cast<QComboBox*>(CreateDeviceCombo());
        connect(l2cs_device_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SettingsDialog::OnDeviceChanged);
        form->addRow(QStringLiteral("推理设备:"), l2cs_device_combo_);
        break;
    }
    case AnalysisBackend::kMefargAu: {
        mefarg_model_combo_ = MakeCombo(this, {QStringLiteral("ResNet18")});
        mefarg_model_combo_->setItemData(0, QStringLiteral("models/OpenGprahAU/mefarg_resnet18_au.onnx"));
        form->addRow(QStringLiteral("模型:"), mefarg_model_combo_);

        mefarg_threshold_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
        mefarg_threshold_spin_->setValue(0.50);
        form->addRow(QStringLiteral("AU 激活阈值:"), mefarg_threshold_spin_);

        mefarg_draw_au_cb_ = MakeCheckBox(QStringLiteral("绘制 AU 信息"), page);
        mefarg_draw_au_cb_->setChecked(true);
        form->addRow({}, mefarg_draw_au_cb_);

        mefarg_device_combo_ = qobject_cast<QComboBox*>(CreateDeviceCombo());
        connect(mefarg_device_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SettingsDialog::OnDeviceChanged);
        form->addRow(QStringLiteral("推理设备:"), mefarg_device_combo_);
        break;
    }
    case AnalysisBackend::kNone: {
        auto* label = new QLabel(QStringLiteral("无需额外配置"), page);
        label->setStyleSheet(QString("QLabel { color: %1; padding: 12px 0; }").arg(ThemeManager::Instance().TextDim()));
        form->addRow(label);
        break;
    }
    }
    return page;
}

// ═══════════════════════════════════════════════════════════════
// 高级参数
// ═══════════════════════════════════════════════════════════════

QWidget* SettingsDialog::CreateAdvancedPanel() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // ── Detection ────────────────────────────────────────────

    auto* detGroup = new QGroupBox(QStringLiteral("检测参数"), page);
    detGroup->setStyleSheet(GroupBoxStyle());
    auto* detLayout = new QFormLayout(detGroup);
    detLayout->setSpacing(8);

    det_input_w_spin_ = MakeSpinBox(this, 64, 2048);
    det_input_w_spin_->setValue(640);
    detLayout->addRow(QStringLiteral("输入宽度:"), det_input_w_spin_);

    det_input_h_spin_ = MakeSpinBox(this, 64, 2048);
    det_input_h_spin_->setValue(640);
    detLayout->addRow(QStringLiteral("输入高度:"), det_input_h_spin_);

    min_face_area_spin_ = MakeDoubleSpin(this, 0.0, 10000.0, 100.0, 0);
    min_face_area_spin_->setSpecialValueText(QStringLiteral("不限"));
    detLayout->addRow(QStringLiteral("最小人脸面积:"), min_face_area_spin_);

    det_conf_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
    det_conf_spin_->setValue(0.50);
    detLayout->addRow(QStringLiteral("置信度阈值:"), det_conf_spin_);

    det_nms_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
    det_nms_spin_->setValue(0.40);
    detLayout->addRow(QStringLiteral("NMS IoU 阈值:"), det_nms_spin_);

    form->addRow(detGroup);

    // ── Tracking ─────────────────────────────────────────────

    auto* trackGroup = new QGroupBox(QStringLiteral("跟踪参数"), page);
    trackGroup->setStyleSheet(GroupBoxStyle());
    auto* trackLayout = new QFormLayout(trackGroup);
    trackLayout->setSpacing(8);

    track_high_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
    track_high_spin_->setValue(0.50);
    trackLayout->addRow(QStringLiteral("高分阈值:"), track_high_spin_);

    track_match_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
    track_match_spin_->setValue(0.40);
    trackLayout->addRow(QStringLiteral("匹配阈值:"), track_match_spin_);

    track_match2_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
    track_match2_spin_->setValue(0.50);
    trackLayout->addRow(QStringLiteral("二次匹配阈值:"), track_match2_spin_);

    track_new_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
    track_new_spin_->setValue(0.70);
    trackLayout->addRow(QStringLiteral("新轨道阈值:"), track_new_spin_);

    form->addRow(trackGroup);

    // ── Recognition ──────────────────────────────────────────

    auto* recGroup = new QGroupBox(QStringLiteral("识别参数"), page);
    recGroup->setStyleSheet(GroupBoxStyle());
    auto* recLayout = new QFormLayout(recGroup);
    recLayout->setSpacing(8);

    rec_iou_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
    rec_iou_spin_->setValue(0.30);
    recLayout->addRow(QStringLiteral("识别 IoU 阈值:"), rec_iou_spin_);

    form->addRow(recGroup);

    // ── Behavior ─────────────────────────────────────────────

    auto* behGroup = new QGroupBox(QStringLiteral("行为参数"), page);
    behGroup->setStyleSheet(GroupBoxStyle());
    auto* behLayout = new QFormLayout(behGroup);
    behLayout->setSpacing(8);

    ema_alpha_spin_ = MakeDoubleSpin(this, 0.0, 1.0, 0.05, 2);
    ema_alpha_spin_->setValue(0.70);
    behLayout->addRow(QStringLiteral("EMA 平滑系数 α:"), ema_alpha_spin_);

    max_frame_width_spin_ = MakeSpinBox(this, 0, 4096);
    max_frame_width_spin_->setSpecialValueText(QStringLiteral("不限"));
    behLayout->addRow(QStringLiteral("最大帧宽度:"), max_frame_width_spin_);

    target_recheck_spin_ = MakeSpinBox(this, 0, 300);
    target_recheck_spin_->setSpecialValueText(QStringLiteral("关闭"));
    behLayout->addRow(QStringLiteral("目标复查间隔:"), target_recheck_spin_);

    form->addRow(behGroup);

    return page;
}

// ═══════════════════════════════════════════════════════════════
// Load / Save
// ═══════════════════════════════════════════════════════════════

void SettingsDialog::LoadToUI() {
    const auto& m = config_.model;
    const auto& t = config_.thresholds;
    const auto& b = config_.behavior;

    match_threshold_spin_->setValue(t.match_threshold);
    device_combo_->setCurrentIndex(static_cast<int>(m.device));
    log_level_combo_->setCurrentText(QString::fromStdString(config_.log.level));

    backend_combo_->setCurrentIndex(static_cast<int>(config_.analysis_backend));

    // OpenFace
    const auto& of = config_.openface;
    if (of_profile_combo_) {
        of_profile_combo_->setCurrentText(QString::fromStdString(of.profile));
        of_sigma_spin_->setValue(of.sigma);
        of_reg_spin_->setValue(of.reg_factor);
        of_weight_spin_->setValue(of.weight_factor);
        of_optimise_spin_->setValue(of.optimise_iterations);
        of_template_cb_->setChecked(of.use_face_template > 0);
        of_template_scale_spin_->setValue(of.face_template_scale);
        of_validate_cb_->setChecked(of.validate_detections > 0);
        of_boundary_spin_->setValue(of.validation_boundary);
        of_refine_hier_cb_->setChecked(of.refine_hierarchical > 0);
        of_refine_param_cb_->setChecked(of.refine_parameters > 0);
        of_multiview_cb_->setChecked(of.multi_view_first_frame > 0);
        of_reinit_spin_->setValue(of.reinit_video_every);
        of_limit_pose_cb_->setChecked(of.limit_pose > 0);
    }

    // MediaPipe
    const auto& onx = config_.onnx;
    if (mp_enable_pose_cb_) {
        mp_enable_pose_cb_->setChecked(onx.enable_pose);
        mp_margin_spin_->setValue(onx.margin);
        mp_score_spin_->setValue(onx.score_threshold);
        mp_draw_landmarks_cb_->setChecked(onx.draw_landmarks);
        mp_draw_wireframe_cb_->setChecked(onx.draw_face_wireframe);
        mp_draw_normal_cb_->setChecked(onx.draw_face_normal);
        mp_device_combo_->setCurrentIndex(static_cast<int>(onx.device));
        {
            int mpIdx = mp_model_combo_->findData(QString::fromStdString(onx.landmark_model));
            if (mpIdx < 0) {
                QString stored = QString::fromStdString(onx.landmark_model);
                for (int i = 0; i < mp_model_combo_->count(); ++i) {
                    if (stored.endsWith(mp_model_combo_->itemData(i).toString())) {
                        mpIdx = i;
                        break;
                    }
                }
            }
            if (mpIdx >= 0) mp_model_combo_->setCurrentIndex(mpIdx);
        }
    }

    // L2CS
    const auto& l2 = config_.l2cs;
    if (l2cs_draw_gaze_cb_) {
        l2cs_draw_gaze_cb_->setChecked(l2.draw_gaze);
        l2cs_device_combo_->setCurrentIndex(static_cast<int>(l2.device));
        {
            int l2Idx = l2cs_model_combo_->findData(QString::fromStdString(l2.model));
            if (l2Idx < 0) {
                QString stored = QString::fromStdString(l2.model);
                for (int i = 0; i < l2cs_model_combo_->count(); ++i) {
                    if (stored.endsWith(l2cs_model_combo_->itemData(i).toString())) {
                        l2Idx = i;
                        break;
                    }
                }
            }
            if (l2Idx >= 0) l2cs_model_combo_->setCurrentIndex(l2Idx);
        }
    }

    // MEFARG-AU
    const auto& ma = config_.mefarg_au;
    if (mefarg_threshold_spin_) {
        mefarg_threshold_spin_->setValue(ma.threshold);
        mefarg_draw_au_cb_->setChecked(ma.draw_au);
        mefarg_device_combo_->setCurrentIndex(static_cast<int>(ma.device));
        {
            int maIdx = mefarg_model_combo_->findData(QString::fromStdString(ma.model));
            if (maIdx < 0) {
                QString stored = QString::fromStdString(ma.model);
                for (int i = 0; i < mefarg_model_combo_->count(); ++i) {
                    if (stored.endsWith(mefarg_model_combo_->itemData(i).toString())) {
                        maIdx = i;
                        break;
                    }
                }
            }
            if (maIdx >= 0) mefarg_model_combo_->setCurrentIndex(maIdx);
        }
    }

    // Detection
    det_input_w_spin_->setValue(m.detection_input_width);
    det_input_h_spin_->setValue(m.detection_input_height);
    min_face_area_spin_->setValue(m.min_face_area);
    det_conf_spin_->setValue(t.detection_confidence);
    det_nms_spin_->setValue(t.detection_nms);

    // Tracking
    track_high_spin_->setValue(t.tracking_high);
    track_match_spin_->setValue(t.tracking_match);
    track_match2_spin_->setValue(t.tracking_match_second);
    track_new_spin_->setValue(t.tracking_new);

    // Recognition
    rec_iou_spin_->setValue(m.recognition_iou);

    // Behavior
    ema_alpha_spin_->setValue(b.ema_alpha);
    max_frame_width_spin_->setValue(b.max_frame_width);
    target_recheck_spin_->setValue(b.target_recheck_interval);

    OnBackendChanged(backend_combo_->currentIndex());
}

void SettingsDialog::SaveFromUI() {
    auto& m = config_.model;
    auto& t = config_.thresholds;
    auto& b = config_.behavior;

    t.match_threshold = static_cast<float>(match_threshold_spin_->value());
    m.device = static_cast<InferenceDevice>(device_combo_->currentIndex());
    config_.log.level = log_level_combo_->currentText().toStdString();

    config_.analysis_backend = static_cast<AnalysisBackend>(backend_combo_->currentIndex());

    // OpenFace
    if (of_profile_combo_) {
        auto& of = config_.openface;
        of.profile = of_profile_combo_->currentText().toStdString();
        of.sigma = static_cast<float>(of_sigma_spin_->value());
        of.reg_factor = static_cast<float>(of_reg_spin_->value());
        of.weight_factor = static_cast<float>(of_weight_spin_->value());
        of.optimise_iterations = of_optimise_spin_->value();
        of.use_face_template = of_template_cb_->isChecked() ? 1 : -1;
        of.face_template_scale = static_cast<float>(of_template_scale_spin_->value());
        of.validate_detections = of_validate_cb_->isChecked() ? 1 : -1;
        of.validation_boundary = static_cast<float>(of_boundary_spin_->value());
        of.refine_hierarchical = of_refine_hier_cb_->isChecked() ? 1 : -1;
        of.refine_parameters = of_refine_param_cb_->isChecked() ? 1 : -1;
        of.multi_view_first_frame = of_multiview_cb_->isChecked() ? 1 : -1;
        of.reinit_video_every = of_reinit_spin_->value();
        of.limit_pose = of_limit_pose_cb_->isChecked() ? 1 : -1;
    }

    // MediaPipe
    if (mp_enable_pose_cb_) {
        auto& onx = config_.onnx;
        onx.enable_pose = mp_enable_pose_cb_->isChecked();
        onx.margin = static_cast<float>(mp_margin_spin_->value());
        onx.score_threshold = static_cast<float>(mp_score_spin_->value());
        onx.draw_landmarks = mp_draw_landmarks_cb_->isChecked();
        onx.draw_face_wireframe = mp_draw_wireframe_cb_->isChecked();
        onx.draw_face_normal = mp_draw_normal_cb_->isChecked();
        onx.device = static_cast<InferenceDevice>(mp_device_combo_->currentIndex());
        onx.landmark_model = mp_model_combo_->currentData().toString().toStdString();
    }

    // L2CS
    if (l2cs_draw_gaze_cb_) {
        auto& l2 = config_.l2cs;
        l2.draw_gaze = l2cs_draw_gaze_cb_->isChecked();
        l2.device = static_cast<InferenceDevice>(l2cs_device_combo_->currentIndex());
        l2.model = l2cs_model_combo_->currentData().toString().toStdString();
    }

    // MEFARG-AU
    if (mefarg_threshold_spin_) {
        auto& ma = config_.mefarg_au;
        ma.threshold = static_cast<float>(mefarg_threshold_spin_->value());
        ma.draw_au = mefarg_draw_au_cb_->isChecked();
        ma.device = static_cast<InferenceDevice>(mefarg_device_combo_->currentIndex());
        ma.model = mefarg_model_combo_->currentData().toString().toStdString();
    }

    // Detection
    m.detection_input_width = det_input_w_spin_->value();
    m.detection_input_height = det_input_h_spin_->value();
    m.min_face_area = static_cast<float>(min_face_area_spin_->value());
    t.detection_confidence = static_cast<float>(det_conf_spin_->value());
    t.detection_nms = static_cast<float>(det_nms_spin_->value());

    // Tracking
    t.tracking_high = static_cast<float>(track_high_spin_->value());
    t.tracking_match = static_cast<float>(track_match_spin_->value());
    t.tracking_match_second = static_cast<float>(track_match2_spin_->value());
    t.tracking_new = static_cast<float>(track_new_spin_->value());

    // Recognition
    m.recognition_iou = static_cast<float>(rec_iou_spin_->value());

    // Behavior
    b.ema_alpha = static_cast<float>(ema_alpha_spin_->value());
    b.max_frame_width = max_frame_width_spin_->value();
    b.target_recheck_interval = target_recheck_spin_->value();
}

// ═══════════════════════════════════════════════════════════════
// Slots
// ═══════════════════════════════════════════════════════════════

void SettingsDialog::OnSectionChanged(int row) {
    panels_->setCurrentIndex(row);
}

void SettingsDialog::OnBackendChanged(int index) {
    auto be = static_cast<AnalysisBackend>(index);
    if (backend_params_stack_)
        backend_params_stack_->setCurrentIndex(static_cast<int>(be));
}

void SettingsDialog::OnDeviceChanged(int index) {
    auto device = static_cast<InferenceDevice>(index);
    if (device == InferenceDevice::kCPU) return;

    if (!detail::OnnxSession::CheckProviderAvailable(device)) {
        QComboBox* senderCombo = qobject_cast<QComboBox*>(sender());
        QString deviceName;
        switch (device) {
        case InferenceDevice::kCUDA:     deviceName = QStringLiteral("CUDA"); break;
        case InferenceDevice::kDirectML: deviceName = QStringLiteral("DirectML"); break;
        case InferenceDevice::kTensorRT: deviceName = QStringLiteral("TensorRT"); break;
        default: break;
        }
        QMessageBox::warning(this, QStringLiteral("加速不可用"),
            QStringLiteral("%1 加速在当前环境中不可用。\n请确认已安装对应的驱动和运行时。").arg(deviceName));
        if (senderCombo)
            senderCombo->setCurrentIndex(static_cast<int>(InferenceDevice::kCPU));
    }
}

void SettingsDialog::OnResetAll() {
    auto result = QMessageBox::warning(this,
        QStringLiteral("恢复默认设置"),
        QStringLiteral("确定要将所有参数恢复为默认值吗？\n\n此操作将重置所有设置项。"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (result != QMessageBox::Yes) return;
    config_ = {};
    LoadToUI();
}

void SettingsDialog::OnAccepted() {
    SaveFromUI();
    emit ConfigChanged(config_);
    QMessageBox::information(this, QStringLiteral("保存成功"),
        QStringLiteral("参数设置已成功保存。"), QMessageBox::Ok);
    accept();
}

}  // namespace ui
}  // namespace omniface
