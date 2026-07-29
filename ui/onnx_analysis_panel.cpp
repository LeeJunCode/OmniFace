#include "omniface/ui/onnx_analysis_panel.h"

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

namespace omniface::ui {

OnnxAnalysisPanel::OnnxAnalysisPanel(QWidget* parent)
    : QWidget(parent) {
    setStyleSheet("background-color: #ffffff; border-radius: 6px; border: 1px solid #e0e5ec;");
    setMinimumWidth(260);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    title_label_ = new QLabel(QStringLiteral("\u5206\u6790\u7ed3\u679c"));
    title_label_->setStyleSheet(
        "color: #333333; font-size: 14px; font-weight: bold; background: transparent;");
    layout->addWidget(title_label_);

    data_label_ = new QLabel(QStringLiteral("\u6682\u65e0\u6570\u636e"));
    data_label_->setStyleSheet(
        "color: #555555; font-size: 13px; "
        "background: transparent; padding: 4px;");
    data_label_->setWordWrap(true);
    data_label_->setTextFormat(Qt::PlainText);
    layout->addWidget(data_label_);

    layout->addStretch();
}

void OnnxAnalysisPanel::setAnalysisLabel(const QString& label) {
    title_label_->setText(label);
}

void OnnxAnalysisPanel::updateResults(
    const std::vector<analysis::onnx::OnnxAnalysisResult>& results) {
    results_ = results;

    if (results.empty()) {
        data_label_->setText(QStringLiteral("\u672a\u68c0\u6d4b\u5230\u4eba\u8138"));
        return;
    }

    QString text;
    int valid_count = 0;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        if (!r.pose_valid) continue;
        valid_count++;

        double p = r.pose.pose.pitch;
        double y = r.pose.pose.yaw;
        double r_ang = r.pose.pose.roll;

        QString pitch_desc, yaw_desc, roll_desc;
        if (p > 18.0) pitch_desc = QStringLiteral("\u4f4e\u5934");
        else if (p > 7.0) pitch_desc = QStringLiteral("\u5fae\u4f4e\u5934");
        else if (p < -18.0) pitch_desc = QStringLiteral("\u62ac\u5934");
        else if (p < -7.0) pitch_desc = QStringLiteral("\u5fae\u62ac\u5934");
        else pitch_desc = QStringLiteral("\u6b63\u89c6");

        if (y > 18.0) yaw_desc = QStringLiteral("\u53f3\u8f6c");
        else if (y > 7.0) yaw_desc = QStringLiteral("\u5fae\u53f3\u8f6c");
        else if (y < -18.0) yaw_desc = QStringLiteral("\u5de6\u8f6c");
        else if (y < -7.0) yaw_desc = QStringLiteral("\u5fae\u5de6\u8f6c");
        else yaw_desc = QStringLiteral("\u6b63\u89c6");

        if (r_ang > 12.0) roll_desc = QStringLiteral("\u53f3\u503e");
        else if (r_ang < -12.0) roll_desc = QStringLiteral("\u5de6\u503e");
        else roll_desc = QStringLiteral("\u6c34\u5e73");

        if (i > 0) text += "\n";
        text += QStringLiteral(
            "\u76ee\u6807\u4eba\u7269\n"
            "  \u9762\u90e8\u671d\u5411: %1\u3001%2\u3001%3\n"
            "  \u5173\u952e\u70b9\u6570: 468 \u4e2a\n"
            "  \u4fef\u4ef0\u89d2: %4\u00b0  \u504f\u822a\u89d2: %5\u00b0  \u7ffb\u6eda\u89d2: %6\u00b0")
            .arg(pitch_desc, yaw_desc, roll_desc)
            .arg(p, 0, 'f', 2)
            .arg(y, 0, 'f', 2)
            .arg(r_ang, 0, 'f', 2);
    }

    if (valid_count == 0) {
        data_label_->setText(
            QStringLiteral("%1 \u4e2a\u4eba\u8138\u5df2\u8ddf\u8e2a\uff0c\u672a\u83b7\u53d6\u5230\u6709\u6548\u59ff\u6001\u6570\u636e")
                .arg(results.size()));
    } else {
        data_label_->setText(text);
    }
}

void OnnxAnalysisPanel::updateGazeResults(
    const std::vector<analysis::onnx::GazeEstimationResult>& results) {
    if (results.empty()) {
        data_label_->setText(QStringLiteral("\u672a\u68c0\u6d4b\u5230\u4eba\u8138"));
        return;
    }

    QString text;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        if (!r.valid) continue;

        float y = r.yaw;
        float p = r.pitch;

        QString h_dir, v_dir;
        if (y > 10.0f) h_dir = QStringLiteral("\u53f3");
        else if (y < -10.0f) h_dir = QStringLiteral("\u5de6");
        else h_dir = QStringLiteral("\u6b63\u89c6");

        if (p > 10.0f) v_dir = QStringLiteral("\u4e0a");
        else if (p < -10.0f) v_dir = QStringLiteral("\u4e0b");
        else v_dir = QStringLiteral("\u6b63\u89c6");

        if (i > 0) text += "\n";
        text += QStringLiteral(
            "\u76ee\u6807\u4eba\u7269\n"
            "  \u89c6\u7ebf\u65b9\u5411: \u6c34\u5e73%1\u3001\u5782\u76f4%2\n"
            "  \u504f\u822a\u89d2: %3\u00b0  \u4fef\u4ef0\u89d2: %4\u00b0\n"
            "  \u89c6\u7ebf\u5411\u91cf: (%5, %6, %7)")
            .arg(h_dir, v_dir)
            .arg(y, 0, 'f', 1)
            .arg(p, 0, 'f', 1)
            .arg(r.gaze_vector[0], 0, 'f', 2)
            .arg(r.gaze_vector[1], 0, 'f', 2)
            .arg(r.gaze_vector[2], 0, 'f', 2);
    }

    data_label_->setText(text.isEmpty() ? QStringLiteral("\u6682\u65e0\u6570\u636e") : text);
}

void OnnxAnalysisPanel::clear() {
    results_.clear();
    data_label_->setText(QStringLiteral("\u6682\u65e0\u6570\u636e"));
}

void OnnxAnalysisPanel::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(0xff, 0xff, 0xff));
    painter.setPen(QPen(QColor(0xe0, 0xe5, 0xec), 1));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
    QWidget::paintEvent(event);
}

QSize OnnxAnalysisPanel::minimumSizeHint() const {
    return QSize(260, 150);
}

}  // namespace omniface::ui
