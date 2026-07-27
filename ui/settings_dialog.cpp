#include "omniface/ui/settings_dialog.h"

#include <thread>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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

}  // namespace

SettingsDialog::SettingsDialog(const PipelineConfig& current, QWidget* parent)
    : QDialog(parent), config_(current) {
    setWindowTitle(QStringLiteral("\u53c2\u6570\u8bbe\u7f6e"));
    setMinimumWidth(540);
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

    root->addWidget(core_group);

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

    root->addWidget(adv_group);

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
}

PipelineConfig SettingsDialog::getConfig() const {
    return config_;
}

}  // namespace omniface::ui
