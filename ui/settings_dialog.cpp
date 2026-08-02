#include "omniface/ui/settings_dialog.h"

#include <thread>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace omniface::ui {

namespace {

constexpr double kStep = 0.05;

QDoubleSpinBox* MakeDoubleBox(double min, double max, double val,
                              int decimals, QWidget* parent = nullptr) {
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(min, max);
    box->setSingleStep(kStep);
    box->setDecimals(decimals);
    box->setValue(val);
    box->setMinimumWidth(120);
    return box;
}

QSpinBox* MakeIntBox(int min, int max, int val, QWidget* parent = nullptr) {
    auto* box = new QSpinBox(parent);
    box->setRange(min, max);
    box->setValue(val);
    box->setMinimumWidth(120);
    return box;
}

QLabel* HintLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setStyleSheet("color: #888888; font-size: 11px; padding-left: 4px;");
    return label;
}

QLabel* WarnLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setStyleSheet("color: #e65100; font-size: 11px; font-weight: bold; padding-left: 4px;");
    return label;
}

QComboBox* makeBoolCombo(bool val) {
    auto* box = new QComboBox;
    box->addItem(QStringLiteral("\u5173\u95ed"), false);
    box->addItem(QStringLiteral("\u5f00\u542f"), true);
    box->setCurrentIndex(val ? 1 : 0);
    box->setMinimumWidth(120);
    return box;
}

QComboBox* MakeDeviceCombo(InferenceDevice current) {
    auto* box = new QComboBox;
    box->addItem(QStringLiteral("CPU"), "cpu");
    box->addItem(QStringLiteral("DirectML"), "directml");
    box->addItem(QStringLiteral("CUDA"), "cuda");
    box->addItem(QStringLiteral("TensorRT"), "tensorrt");
    int idx = box->findData(QString::fromStdString(InferenceDeviceToString(current)));
    if (idx < 0) idx = 0;
    box->setCurrentIndex(idx);
    box->setMinimumWidth(180);

    // Validate initial value: if selected GPU device unavailable, silently fall back to CPU
    if (current != InferenceDevice::kCPU &&
        !detail::OnnxSession::CheckProviderAvailable(current)) {
        box->blockSignals(true);
        box->setCurrentIndex(0);
        box->blockSignals(false);
    }

    QObject::connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), box,
                     [box](int index) {
        QString data = box->itemData(index).toString();
        InferenceDevice selected = InferenceDeviceFromString(data.toStdString());
        if (selected != InferenceDevice::kCPU &&
            !detail::OnnxSession::CheckProviderAvailable(selected)) {
            QMessageBox::warning(
                box->window(),
                QStringLiteral("\u8bbe\u5907\u4e0d\u53ef\u7528"),
                QStringLiteral("\u6240\u9009\u8bbe\u5907 '%1' \u5728\u5f53\u524d\u73af\u5883\u4e2d\u4e0d\u53ef\u7528\uff0c\u5df2\u5207\u6362\u4e3a CPU\u3002")
                    .arg(box->itemText(index)));
            box->blockSignals(true);
            box->setCurrentIndex(0);
            box->blockSignals(false);
        }
    });

    return box;
}

}  // namespace

SettingsDialog::SettingsDialog(const PipelineConfig& current, QWidget* parent)
    : QDialog(parent), config_(current) {
    setWindowTitle(QStringLiteral("\u53c2\u6570\u8bbe\u7f6e"));
    setMinimumSize(580, 520);
    resize(620, 560);
    setStyleSheet("QDialog { background-color: #ffffff; }"
                  "QComboBox QAbstractItemView { background-color: #ffffff; }");
    setupUI();
}

void SettingsDialog::setupUI() {
    unsigned int hw_threads = std::thread::hardware_concurrency();
    if (hw_threads == 0) hw_threads = 4;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 16);
    root->setSpacing(16);

    // ===== 核心参数 =====
    auto* core_group = new QGroupBox(QStringLiteral("\u6838\u5fc3\u53c2\u6570"));
    core_group->setStyleSheet(
        "QGroupBox { font-weight: bold; font-size: 14px; color: #1565c0; "
        "border: 2px solid #e3f2fd; border-radius: 8px; "
        "background-color: #fafbff; margin-top: 12px; padding-top: 18px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 16px; padding: 0 8px; }");

    auto* core_form = new QFormLayout(core_group);
    core_form->setContentsMargins(20, 20, 20, 16);
    core_form->setSpacing(8);

    auto addCoreRow = [&](const QString& label, QWidget* widget, const QString& hint) {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 2, 0, 2);
        row->addWidget(widget);
        if (!hint.isEmpty()) row->addWidget(HintLabel(hint));
        row->addStretch();
        core_form->addRow(new QLabel(label), row);
    };

    match_threshold_ = MakeDoubleBox(0.0, 1.0, config_.thresholds.match_threshold, 3);
    addCoreRow(QStringLiteral("\u4eba\u8138\u5339\u914d\u9608\u503c"), match_threshold_,
               QStringLiteral("\u5efa\u8bae 0.20 ~ 0.40\uff0c\u8d8a\u4f4e\u8d8a\u5bbd\u677e\uff0c\u8d8a\u9ad8\u8d8a\u4e25\u683c"));

    detection_confidence_ = MakeDoubleBox(0.0, 1.0, config_.thresholds.detection_confidence, 2);
    addCoreRow(QStringLiteral("\u68c0\u6d4b\u7f6e\u4fe1\u5ea6"), detection_confidence_,
               QStringLiteral("\u5efa\u8bae 0.40 ~ 0.70"));

    // 线程数：附带硬件线程信息
    auto* dt_container = new QHBoxLayout;
    detection_threads_ = MakeIntBox(1, 32, config_.model.detection_threads);
    dt_container->addWidget(detection_threads_);
    dt_container->addWidget(HintLabel(QStringLiteral("\u5efa\u8bae 1 ~ %1").arg(hw_threads)));
    dt_container->addStretch();
    core_form->addRow(new QLabel(QStringLiteral("\u68c0\u6d4b\u7ebf\u7a0b\u6570")), dt_container);

    auto* rt_container = new QHBoxLayout;
    recognition_threads_ = MakeIntBox(1, 32, config_.model.recognition_threads);
    rt_container->addWidget(recognition_threads_);
    rt_container->addWidget(HintLabel(QStringLiteral("\u5efa\u8bae 1 ~ %1").arg(hw_threads)));
    rt_container->addStretch();
    core_form->addRow(new QLabel(QStringLiteral("\u8bc6\u522b\u7ebf\u7a0b\u6570")), rt_container);

    // 检测+识别模型推理设备
    model_device_ = MakeDeviceCombo(config_.model.device);
    auto* md_container = new QHBoxLayout;
    md_container->addWidget(model_device_);
    md_container->addWidget(HintLabel(QStringLiteral("\u68c0\u6d4b\u3001识\u522b\u6a21\u578b\u8bbe\u5907\uff0c\u4ec5\u5f71\u54cd ONNX \u63a8\u7406")));
    md_container->addStretch();
    core_form->addRow(new QLabel(QStringLiteral("\u6a21\u578b\u63a8\u7406\u8bbe\u5907")), md_container);

    // 硬件线程信息
    auto* hw_label = new QLabel(
        QStringLiteral("\u5f53\u524d\u8ba1\u7b97\u673a\u53ef\u7528\u786c\u4ef6\u7ebf\u7a0b\uff1a%1\u3002\u8d85\u8fc7\u6b64\u503c\u4f1a\u5bfc\u81f4\u7ebf\u7a0b\u7ade\u4e89\uff0c\u53cd\u800c\u964d\u4f4e\u5904\u7406\u901f\u5ea6\u3002")
            .arg(hw_threads));
    hw_label->setStyleSheet("color: #888888; font-size: 11px; padding: 4px 0;");
    hw_label->setWordWrap(true);
    core_form->addRow(hw_label);

    // 实时验证
    auto checkThreads = [this, hw_threads, core_group, dt_container, rt_container]() {
        int dt = detection_threads_->value();
        int rt = recognition_threads_->value();
        bool over = (dt + rt) > static_cast<int>(hw_threads);
        // 移除旧警告
        auto* old_warn = dt_container->parentWidget()->findChild<QLabel*>("threadWarn");
        if (old_warn) old_warn->deleteLater();
        if (over) {
            auto* warn = WarnLabel(
                QStringLiteral("\u26a0 \u68c0\u6d4b+识\u522b\u7ebf\u7a0b\u603b\u6570(%1)超\u8fc7\u786c\u4ef6\u7ebf\u7a0b(%2)\uff0c\u53ef\u80fd\u964d\u4f4e\u6027\u80fd")
                    .arg(dt + rt).arg(hw_threads));
            warn->setObjectName("threadWarn");
            warn->setWordWrap(true);
            auto* form = qobject_cast<QFormLayout*>(core_group->layout());
            if (form) form->addRow(warn);
        }
    };

    connect(detection_threads_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, checkThreads);
    connect(recognition_threads_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, checkThreads);
    checkThreads();

    // ===== 分析后端选择 =====
    auto* backend_container = new QHBoxLayout;
    auto* backend_label = new QLabel(QStringLiteral("\u5206\u6790\u540e\u7aef\uff1a"));
    backend_label->setStyleSheet("font-weight: bold; font-size: 13px; color: #1565c0;");

    analysis_backend_ = new QComboBox;
    analysis_backend_->addItem(QStringLiteral("\u9762\u90e8\u8868\u60c5\u5206\u6790 (OpenFace)"), "openface");
    analysis_backend_->addItem(QStringLiteral("\u5934\u90e8\u59ff\u6001\u63d0\u53d6 (MediaPipe)"), "mediapipe");
    analysis_backend_->addItem(QStringLiteral("\u89c6\u7ebf\u63d0\u53d6 (L2CS-Net)"), "l2cs");
    analysis_backend_->addItem(QStringLiteral("\u65e0"), "none");
    analysis_backend_->setCurrentIndex(
        analysis_backend_->findData(
            AnalysisBackendToString(config_.analysis_backend)));
    analysis_backend_->setMinimumWidth(200);
    backend_container->addWidget(backend_label);
    backend_container->addWidget(analysis_backend_);
    backend_container->addStretch();

    // 分析配置堆栈（根据后端切换显示不同页面）
    auto* config_stack = new QStackedWidget;

    auto syncAnalysisPage = [config_stack, this]() {
        QString backend = analysis_backend_->currentData().toString();
        if (backend == "mediapipe") config_stack->setCurrentIndex(1);
        else if (backend == "l2cs") config_stack->setCurrentIndex(2);
        else if (backend == "none") config_stack->setCurrentIndex(3);
        else config_stack->setCurrentIndex(0);
    };
    connect(analysis_backend_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, syncAnalysisPage);

    // ===== 高级参数 =====
    auto* adv_group = new QGroupBox(QStringLiteral("\u9ad8\u7ea7\u53c2\u6570"));
    adv_group->setStyleSheet(
        "QGroupBox { font-weight: bold; font-size: 13px; color: #757575; "
        "border: 1px solid #e0e0e0; border-radius: 6px; "
        "background-color: #fafafa; margin-top: 10px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 16px; padding: 0 8px; }");

    auto* adv_form = new QFormLayout(adv_group);
    adv_form->setContentsMargins(20, 16, 20, 12);
    adv_form->setSpacing(4);

    auto addAdvRow = [&](const QString& label, QWidget* widget, const QString& hint) {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 1, 0, 1);
        row->addWidget(widget);
        if (!hint.isEmpty()) row->addWidget(HintLabel(hint));
        row->addStretch();
        adv_form->addRow(new QLabel(label), row);
    };

    detection_nms_ = MakeDoubleBox(0.0, 1.0, config_.thresholds.detection_nms, 2);
    addAdvRow(QStringLiteral("NMS \u53e0\u52a0\u9608\u503c"), detection_nms_,
              QStringLiteral("\u5efa\u8bae 0.30 ~ 0.50"));

    min_face_area_ = MakeDoubleBox(0.0, 100000.0, config_.model.min_face_area, 0);
    addAdvRow(QStringLiteral("\u6700\u5c0f\u4eba\u8138\u9762\u79ef"), min_face_area_,
              QStringLiteral("0 = \u4e0d\u8fc7\u6ee4"));

    recognition_confidence_threshold_ = MakeDoubleBox(0.0, 1.0,
        config_.model.recognition_confidence_threshold, 2);
    addAdvRow(QStringLiteral("\u8bc6\u522b\u7f6e\u4fe1\u5ea6\u8fc7\u6ee4"), recognition_confidence_threshold_,
              QStringLiteral("0 = \u4e0d\u8fc7\u6ee4"));

    recognition_iou_ = MakeDoubleBox(0.0, 1.0, config_.model.recognition_iou, 2);
    addAdvRow(QStringLiteral("\u8bc6\u522b IoU \u9608\u503c"), recognition_iou_,
              QStringLiteral("\u5efa\u8bae 0.20 ~ 0.40"));

    tracking_high_ = MakeDoubleBox(0.0, 1.0, config_.thresholds.tracking_high, 2);
    addAdvRow(QStringLiteral("\u8ddf\u8e2a-\u9996\u6b21\u5173\u8054"), tracking_high_,
              QStringLiteral("\u5efa\u8bae 0.40 ~ 0.60"));

    tracking_match_ = MakeDoubleBox(0.0, 1.0, config_.thresholds.tracking_match, 2);
    addAdvRow(QStringLiteral("\u8ddf\u8e2a-\u4e8c\u6b21\u5173\u8054"), tracking_match_,
              QStringLiteral("\u5efa\u8bae 0.30 ~ 0.50"));

    tracking_match_second_ = MakeDoubleBox(0.0, 1.0,
        config_.thresholds.tracking_match_second, 2);
    addAdvRow(QStringLiteral("\u8ddf\u8e2a-\u4f4e\u5206\u5173\u8054"), tracking_match_second_,
              QStringLiteral("\u5efa\u8bae 0.40 ~ 0.60"));

    tracking_new_ = MakeDoubleBox(0.0, 1.0, config_.thresholds.tracking_new, 2);
    addAdvRow(QStringLiteral("\u8ddf\u8e2a-\u65b0\u8f68\u8ff9\u9608\u503c"), tracking_new_,
              QStringLiteral("\u5efa\u8bae 0.60 ~ 0.80"));

    ema_alpha_ = MakeDoubleBox(0.0, 1.0, config_.behavior.ema_alpha, 2);
    addAdvRow(QStringLiteral("FPS \u5e73\u6ed1\u7cfb\u6570"), ema_alpha_,
              QStringLiteral("\u8d8a\u9ad8\u8d8a\u5e73\u6ed1\uff0c\u5efa\u8bae 0.60 ~ 0.80"));

    max_frame_width_ = MakeIntBox(0, 3840, config_.behavior.max_frame_width);
    max_frame_width_->setSpecialValueText(QStringLiteral("\u4e0d\u9650\u5236"));
    addAdvRow(QStringLiteral("\u6700\u5927\u5e27\u5bbd\u5ea6"), max_frame_width_,
              QStringLiteral("0 = \u4e0d\u9650\u5236\uff0c\u7f29\u5c0f\u53ef\u63d0\u5347\u6027\u80fd"));

    log_level_ = new QComboBox;
    log_level_->addItems({"debug", "info", "warn", "error"});
    log_level_->setCurrentText(QString::fromStdString(config_.log.level));
    log_level_->setMinimumWidth(120);
    addAdvRow(QStringLiteral("\u65e5\u5fd7\u7ea7\u522b"), log_level_,
              QStringLiteral("debug=\u6700\u8be6\u7ec6\uff0cinfo=\u4e00\u822c\uff0cwarn=\u8b66\u544a"));

    // ===== OpenFace 参数 =====
    auto* of_group = new QGroupBox(QStringLiteral("OpenFace \u53c2\u6570"));
    of_group->setStyleSheet(
        "QGroupBox { font-weight: bold; font-size: 13px; color: #6a1b9a; "
        "border: 2px solid #f3e5f5; border-radius: 8px; "
        "background-color: #fdfbff; margin-top: 12px; padding-top: 18px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 16px; padding: 0 8px; }");

    auto* of_form = new QFormLayout(of_group);
    of_form->setContentsMargins(20, 16, 20, 12);
    of_form->setSpacing(8);

    auto addOfRow = [&](const QString& label, QWidget* widget, const QString& hint) {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 2, 0, 2);
        row->addWidget(widget);
        if (!hint.isEmpty()) row->addWidget(HintLabel(hint));
        row->addStretch();
        of_form->addRow(new QLabel(label), row);
    };

    of_profile_ = new QComboBox;
    const QStringList profile_names = {"standard", "max_quality"};
    const QStringList profile_labels = {
        QStringLiteral("Standard (\u6807\u51c6)"), QStringLiteral("Max Quality (\u6700\u9ad8\u7cbe\u5ea6)")};
    for (int i = 0; i < profile_names.size(); ++i)
        of_profile_->addItem(profile_labels[i], profile_names[i]);
    int prof_idx = profile_names.indexOf(QString::fromStdString(config_.openface.profile));
    of_profile_->setCurrentIndex(prof_idx >= 0 ? prof_idx : 1);
    of_profile_->setMinimumWidth(200);
    addOfRow(QStringLiteral("\u9884\u8bbe\u65b9\u6848"), of_profile_,
             QStringLiteral("\u63a7\u5236\u7b97\u6cd5\u901f\u5ea6\u4e0e\u7cbe\u5ea6\u5e73\u8861"));

    // Profile 描述提示
    auto updateOfHint = [this, of_form]() {
        auto* old = of_form->parentWidget()->findChild<QLabel*>("ofHint");
        if (old) old->deleteLater();
        QString hint;
        QString prof = of_profile_->currentData().toString();
        if (prof == "standard")
            hint = QStringLiteral("\u25b6 CE-CLM + 5\u6b21\u8fed\u4ee3 + \u9762\u90e8\u6a21\u677f\uff0c\u7cbe\u5ea6\u4e0e\u901f\u5ea6\u5747\u8861\uff08\u63a8\u8350\uff09");
        else if (prof == "max_quality")
            hint = QStringLiteral("\u25b6 10\u6b21\u8fed\u4ee3 + NU-RLMS + \u5927\u7a97\u53e3 + \u591a\u89c6\u56fe\uff0c\u6700\u9ad8\u7cbe\u5ea6\uff0c\u901f\u5ea6\u8f83\u6162");
        auto* h = HintLabel(hint);
        h->setObjectName("ofHint");
        h->setWordWrap(true);
        of_form->addRow(h);
    };
    connect(of_profile_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, updateOfHint);
    updateOfHint();

    // --- Page 0: OpenFace (already built as of_group above) ---
    config_stack->addWidget(of_group);

    // --- Page 1: ONNX config ---
    auto* onnx_page = new QWidget;
    auto* onnx_form = new QFormLayout(onnx_page);
    onnx_form->setContentsMargins(0, 8, 0, 0);
    onnx_form->setSpacing(6);

    auto addOnnxRow = [&](const QString& label, QWidget* widget, const QString& hint) {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 2, 0, 2);
        row->addWidget(widget);
        if (!hint.isEmpty()) row->addWidget(HintLabel(hint));
        row->addStretch();
        onnx_form->addRow(new QLabel(label), row);
    };

    onnx_threads_ = MakeIntBox(1, 32, config_.onnx.analysis_threads);
    addOnnxRow(QStringLiteral("\u5206\u6790\u7ebf\u7a0b\u6570"), onnx_threads_,
               QStringLiteral("ONNX\u63a8\u7406\u7ebf\u7a0b\u6570"));

    onnx_margin_ = MakeDoubleBox(0.0, 1.0, config_.onnx.margin, 2);
    addOnnxRow(QStringLiteral("\u8fb9\u6846\u6269\u5c55\u6bd4\u4f8b"), onnx_margin_,
               QStringLiteral("\u4eba\u8138\u88c1\u526a\u8fb9\u6846\u6269\u5c55\u6bd4\u4f8b"));

    onnx_score_threshold_ = MakeDoubleBox(0.0, 1.0, config_.onnx.score_threshold, 2);
    addOnnxRow(QStringLiteral("\u7f6e\u4fe1\u5ea6\u9608\u503c"), onnx_score_threshold_,
               QStringLiteral("\u4ec5\u663e\u793a\u9ad8\u4e8e\u6b64\u503c\u7684\u68c0\u6d4b\u7ed3\u679c"));

    onnx_draw_landmarks_ = makeBoolCombo(config_.onnx.draw_landmarks);
    addOnnxRow(QStringLiteral("\u663e\u793a\u5173\u952e\u70b9 (468)"), onnx_draw_landmarks_,
               QStringLiteral("\u5728\u89c6\u9891\u5e27\u4e0a\u7ed8\u5236\u9762\u90e8\u7f51\u683c\u5173\u952e\u70b9"));

    onnx_draw_wireframe_ = makeBoolCombo(config_.onnx.draw_face_wireframe);
    addOnnxRow(QStringLiteral("\u663e\u793a\u7ebf\u6846\u6a21\u578b"), onnx_draw_wireframe_,
               QStringLiteral("\u7ed8\u5236 3D \u4eba\u8138\u7ebf\u6846\u6295\u5f71"));

    onnx_draw_normal_ = makeBoolCombo(config_.onnx.draw_face_normal);
    addOnnxRow(QStringLiteral("\u663e\u793a\u6cd5\u7ebf\u7bad\u5934"), onnx_draw_normal_,
               QStringLiteral("\u7ed8\u5236\u9f3b\u5c16\u6cd5\u7ebf\u65b9\u5411\u7bad\u5934"));

    onnx_device_ = MakeDeviceCombo(config_.onnx.device);
    addOnnxRow(QStringLiteral("\u63a8\u7406\u8bbe\u5907"), onnx_device_,
               QStringLiteral("MediaPipe \u6a21\u578b GPU \u52a0\u901f"));

    config_stack->addWidget(onnx_page);

    // --- Page 2: Gaze config ---
    auto* gaze_page = new QWidget;
    auto* gaze_form = new QFormLayout(gaze_page);
    gaze_form->setContentsMargins(0, 8, 0, 0);
    gaze_form->setSpacing(6);

    auto addGazeRow = [&](const QString& label, QWidget* widget, const QString& hint) {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 2, 0, 2);
        row->addWidget(widget);
        if (!hint.isEmpty()) row->addWidget(HintLabel(hint));
        row->addStretch();
        gaze_form->addRow(new QLabel(label), row);
    };

    gaze_model_combo_ = new QComboBox;
    gaze_model_combo_->addItem(QStringLiteral("MobileOne-S0"), "mobileone_s0_gaze.onnx");
    gaze_model_combo_->addItem(QStringLiteral("MobileNetV2"), "mobilenetv2_gaze.onnx");
    gaze_model_combo_->addItem(QStringLiteral("ResNet50"), "resnet50_gaze.onnx");
    int model_idx = gaze_model_combo_->findData(QString::fromStdString(config_.l2cs.model));
    gaze_model_combo_->setCurrentIndex(model_idx >= 0 ? model_idx : 0);
    gaze_model_combo_->setMinimumWidth(200);
    addGazeRow(QStringLiteral("\u6a21\u578b\u9009\u62e9"), gaze_model_combo_,
               QStringLiteral("MobileOne-S0 \u8f83\u5c0f\u8f83\u5feb\uff0cMobileNetV2 \u5747\u8861\uff0cResNet50 \u7cbe\u5ea6\u6700\u9ad8"));

    gaze_draw_ = makeBoolCombo(config_.l2cs.draw_gaze);
    addGazeRow(QStringLiteral("\u663e\u793a\u89c6\u7ebf\u7bad\u5934"), gaze_draw_,
               QStringLiteral("\u5728\u89c6\u9891\u5e27\u4e0a\u7ed8\u5236\u89c6\u7ebf\u65b9\u5411\u7bad\u5934"));

    gaze_device_ = MakeDeviceCombo(config_.l2cs.device);
    addGazeRow(QStringLiteral("\u63a8\u7406\u8bbe\u5907"), gaze_device_,
               QStringLiteral("L2CS-Net \u6a21\u578b GPU \u52a0\u901f"));

    config_stack->addWidget(gaze_page);

    // --- Page 3: None (empty) ---
    config_stack->addWidget(new QWidget);

    syncAnalysisPage();

    // ===== Assemble tabs =====
    auto* tabs = new QTabWidget;
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #e0e0e0; border-radius: 6px; "
        "background-color: #ffffff; }"
        "QTabBar::tab { padding: 8px 20px; font-size: 13px; "
        "border: 1px solid #e0e0e0; border-bottom: none; "
        "border-top-left-radius: 6px; border-top-right-radius: 6px; "
        "background-color: #f5f5f5; margin-right: 2px; }"
        "QTabBar::tab:selected { background-color: #ffffff; "
        "font-weight: bold; color: #1565c0; }");
    tabs->addTab(core_group, QStringLiteral("\u6838\u5fc3\u53c2\u6570"));
    tabs->addTab(adv_group, QStringLiteral("\u9ad8\u7ea7\u53c2\u6570"));
    tabs->addTab(config_stack, QStringLiteral("\u5206\u6790"));
    root->addLayout(backend_container);
    root->addWidget(tabs);

    // ===== 按钮 =====
    auto* btn_row = new QHBoxLayout;

    auto* reset_btn = new QPushButton(QStringLiteral("\u6062\u590d\u9ed8\u8ba4\u503c"));
    reset_btn->setMinimumWidth(120);
    reset_btn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border: 1px solid #c0c0c0; "
        "border-radius: 4px; padding: 6px 14px; color: #757575; }"
        "QPushButton:hover { background-color: #f5f5f5; border-color: #e53935; color: #c62828; }");
    connect(reset_btn, &QPushButton::clicked, this, [this]() {
        PipelineConfig defaults;
        match_threshold_->setValue(defaults.thresholds.match_threshold);
        detection_confidence_->setValue(defaults.thresholds.detection_confidence);
        detection_threads_->setValue(defaults.model.detection_threads);
        recognition_threads_->setValue(defaults.model.recognition_threads);
        detection_nms_->setValue(defaults.thresholds.detection_nms);
        min_face_area_->setValue(defaults.model.min_face_area);
        recognition_confidence_threshold_->setValue(
            defaults.model.recognition_confidence_threshold);
        recognition_iou_->setValue(defaults.model.recognition_iou);
        tracking_high_->setValue(defaults.thresholds.tracking_high);
        tracking_match_->setValue(defaults.thresholds.tracking_match);
        tracking_match_second_->setValue(defaults.thresholds.tracking_match_second);
        tracking_new_->setValue(defaults.thresholds.tracking_new);
        ema_alpha_->setValue(defaults.behavior.ema_alpha);
        max_frame_width_->setValue(defaults.behavior.max_frame_width);
        log_level_->setCurrentText(
            QString::fromStdString(defaults.log.level));

        // OpenFace 默认值
        int of_prof_idx = of_profile_->findData(
            QString::fromStdString(defaults.openface.profile));
        of_profile_->setCurrentIndex(std::max(0, of_prof_idx));

        // ONNX 默认值
        onnx_threads_->setValue(defaults.onnx.analysis_threads);
        onnx_margin_->setValue(defaults.onnx.margin);
        onnx_score_threshold_->setValue(defaults.onnx.score_threshold);
        onnx_draw_landmarks_->setCurrentIndex(defaults.onnx.draw_landmarks ? 1 : 0);
        onnx_draw_wireframe_->setCurrentIndex(defaults.onnx.draw_face_wireframe ? 1 : 0);
        onnx_draw_normal_->setCurrentIndex(defaults.onnx.draw_face_normal ? 1 : 0);
        // 视线估计默认值
        int gmodel_idx = gaze_model_combo_->findData(
            QString::fromStdString(defaults.l2cs.model));
        gaze_model_combo_->setCurrentIndex(gmodel_idx >= 0 ? gmodel_idx : 0);
        gaze_draw_->setCurrentIndex(defaults.l2cs.draw_gaze ? 1 : 0);

        // 推理设备默认值
        auto resetDeviceCombo = [](QComboBox* box, InferenceDevice dev) {
            if (!box) return;
            int idx = box->findData(
                QString::fromStdString(InferenceDeviceToString(dev)));
            box->setCurrentIndex(idx >= 0 ? idx : 0);
        };
        resetDeviceCombo(model_device_, defaults.model.device);
        resetDeviceCombo(onnx_device_, defaults.onnx.device);
        resetDeviceCombo(gaze_device_, defaults.l2cs.device);

        analysis_backend_->setCurrentIndex(
            analysis_backend_->findData(
                AnalysisBackendToString(defaults.analysis_backend)));
    });
    btn_row->addWidget(reset_btn);
    btn_row->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("\u4fdd\u5b58"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("\u53d6\u6d88"));
    buttons->button(QDialogButtonBox::Ok)->setMinimumWidth(100);
    buttons->button(QDialogButtonBox::Cancel)->setMinimumWidth(100);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        saveTo(config_);
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    btn_row->addWidget(buttons);

    root->addLayout(btn_row);
}

void SettingsDialog::saveTo(PipelineConfig& cfg) const {
    cfg.thresholds.match_threshold =
        static_cast<float>(match_threshold_->value());
    cfg.thresholds.detection_confidence =
        static_cast<float>(detection_confidence_->value());
    cfg.model.detection_threads = detection_threads_->value();
    cfg.model.recognition_threads = recognition_threads_->value();

    cfg.thresholds.detection_nms =
        static_cast<float>(detection_nms_->value());
    cfg.model.min_face_area = static_cast<float>(min_face_area_->value());
    cfg.model.recognition_confidence_threshold =
        static_cast<float>(recognition_confidence_threshold_->value());
    cfg.model.recognition_iou =
        static_cast<float>(recognition_iou_->value());
    cfg.thresholds.tracking_high =
        static_cast<float>(tracking_high_->value());
    cfg.thresholds.tracking_match =
        static_cast<float>(tracking_match_->value());
    cfg.thresholds.tracking_match_second =
        static_cast<float>(tracking_match_second_->value());
    cfg.thresholds.tracking_new =
        static_cast<float>(tracking_new_->value());

    cfg.behavior.ema_alpha = static_cast<float>(ema_alpha_->value());
    cfg.behavior.max_frame_width = max_frame_width_->value();
    cfg.log.level = log_level_->currentText().toStdString();

    // OpenFace 配置
    cfg.openface.profile = of_profile_->currentData().toString().toStdString();

    // 模型推理设备
    cfg.model.device = InferenceDeviceFromString(
        model_device_->currentData().toString().toStdString());

    // 分析后端
    cfg.analysis_backend = AnalysisBackendFromString(
        analysis_backend_->currentData().toString().toStdString());

    // ONNX
    cfg.onnx.analysis_threads = onnx_threads_->value();
    cfg.onnx.margin = static_cast<float>(onnx_margin_->value());
    cfg.onnx.score_threshold = static_cast<float>(onnx_score_threshold_->value());
    cfg.onnx.draw_landmarks = onnx_draw_landmarks_->currentData().toBool();
    cfg.onnx.draw_face_wireframe = onnx_draw_wireframe_->currentData().toBool();
    cfg.onnx.draw_face_normal = onnx_draw_normal_->currentData().toBool();
    cfg.onnx.device = InferenceDeviceFromString(
        onnx_device_->currentData().toString().toStdString());
    // 视线估计
    cfg.l2cs.model = gaze_model_combo_->currentData().toString().toStdString();
    cfg.l2cs.draw_gaze = gaze_draw_->currentData().toBool();
    cfg.l2cs.device = InferenceDeviceFromString(
        gaze_device_->currentData().toString().toStdString());
}

PipelineConfig SettingsDialog::getConfig() const {
    return config_;
}

}  // namespace omniface::ui
