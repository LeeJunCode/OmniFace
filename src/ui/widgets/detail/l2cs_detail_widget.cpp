#include "omniface/ui/widgets/detail/l2cs_detail_widget.h"
#include "omniface/ui/theme.h"

#include <cmath>
#include <QFile>
#include <QVBoxLayout>
#include <QTextStream>

namespace omniface {
namespace ui {

namespace {

QString GazeXLabel(float yaw) {
    if (yaw < -25) return QStringLiteral("明显向左看");
    if (yaw < -10) return QStringLiteral("略微向左看");
    if (yaw > 25)  return QStringLiteral("明显向右看");
    if (yaw > 10)  return QStringLiteral("略微向右看");
    return {};
}

QString GazeYLabel(float pitch) {
    if (pitch < -25) return QStringLiteral("明显向下看");
    if (pitch < -10) return QStringLiteral("略微向下看");
    if (pitch > 25)  return QStringLiteral("明显向上看");
    if (pitch > 10)  return QStringLiteral("略微向上看");
    return {};
}

} // anonymous namespace

L2csDetailWidget::L2csDetailWidget(QWidget* parent)
    : BaseDetailWidget(parent) {
    SetupUI();
}

void L2csDetailWidget::SetupUI() {
    auto& tm = ThemeManager::Instance();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    frame_no_label_ = new QLabel(QStringLiteral("当前帧: —"), this);
    frame_no_label_->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-weight: bold; font-size: 13px; }")
        .arg(tm.Primary()));
    root->addWidget(frame_no_label_);

    text_display_ = new QTextEdit(this);
    text_display_->setReadOnly(true);
    text_display_->setStyleSheet(QStringLiteral(
        "QTextEdit { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 8px; font-size: 12px; }")
        .arg(tm.DeepBg(), tm.TextPrimary(), tm.Border()));
    root->addWidget(text_display_, 1);
}

void L2csDetailWidget::LoadCsvData() {
    frames_.clear();
    frame_to_index_.clear();

    QFile file(csv_path_);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.readLine(); // skip header
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList cols = line.split(QStringLiteral(","));
        // CSV: frame,timestamp,yaw_maxp,pitch_maxp,gaze_yaw,gaze_pitch,gaze_x,gaze_y,gaze_z
        if (cols.size() < 9) continue;
        L2csFrameData d;
        d.frame_number = cols.value(0).toInt();
        d.yaw_maxp  = cols.value(2).toFloat();
        d.pitch_maxp = cols.value(3).toFloat();
        d.yaw    = cols.value(4).toFloat();
        d.pitch  = cols.value(5).toFloat();
        d.gaze_x = cols.value(6).toFloat();
        d.gaze_y = cols.value(7).toFloat();
        d.gaze_z = cols.value(8).toFloat();
        frame_to_index_[d.frame_number] = static_cast<int>(frames_.size());
        frames_.push_back(d);
    }
}

int L2csDetailWidget::TotalFrames() const {
    return static_cast<int>(frames_.size());
}

void L2csDetailWidget::SetFrameGrades(const std::unordered_map<int, char>& grades) {
    frame_grades_ = grades;
}

void L2csDetailWidget::UpdateForFrame(int videoFrameNumber) {
    auto& tm = ThemeManager::Instance();
    auto primary = tm.Primary();
    auto textPrimary = tm.TextPrimary();
    auto textDim = tm.TextDim();

    auto it = frame_to_index_.find(videoFrameNumber);

    if (it == frame_to_index_.end()) {
        frame_no_label_->setText(QStringLiteral("当前帧: %1").arg(videoFrameNumber));
        text_display_->setText(QStringLiteral("<p style='color:%1;'>"
            "（当前帧未分析到目标人物数据）</p>").arg(textDim));
        return;
    }

    const auto& d = frames_[it->second];

    frame_no_label_->setText(QStringLiteral("当前帧: %1").arg(videoFrameNumber));

    // Grade badge
    QString grade_str;
    auto git = frame_grades_.find(videoFrameNumber);
    if (git != frame_grades_.end()) {
        QString color;
        switch (git->second) {
        case 'S': color = QStringLiteral("#2196F3"); break;
        case 'A': color = QStringLiteral("#4CAF50"); break;
        case 'B': color = QStringLiteral("#FF9800"); break;
        case 'C': color = QStringLiteral("#FF5722"); break;
        default:  color = QStringLiteral("#F44336"); break;
        }
        grade_str = QStringLiteral(" <span style='color:%1;font-size:12px;'>[%2级]</span>")
            .arg(color).arg(QChar(git->second));
    }

    QString html;
    html += QStringLiteral("<h3 style='color:%1;margin-bottom:10px;'>L2CS-Net 视线分析%2</h3>")
        .arg(primary, grade_str);

    // ── Gaze direction ──
    QString xLabel = GazeXLabel(d.yaw);
    QString yLabel = GazeYLabel(d.pitch);

    html += QStringLiteral("<p style='margin-bottom:8px;'><b style='color:%1;'>视线方向：</b>")
        .arg(textPrimary);
    if (xLabel.isEmpty() && yLabel.isEmpty()) {
        html += QStringLiteral("正视前方");
    } else {
        QStringList parts;
        if (!yLabel.isEmpty()) parts << yLabel;
        if (!xLabel.isEmpty()) parts << xLabel;
        html += parts.join(QStringLiteral("，"));
    }
    html += QStringLiteral("</p>");

    html += QStringLiteral("<table style='color:%1;font-size:12px;margin-bottom:8px;'>"
        "<tr><td>水平偏转角 (yaw)：</td><td style='color:%2;text-align:right;'>%3°</td></tr>"
        "<tr><td>垂直偏转角 (pitch)：</td><td style='color:%2;text-align:right;'>%4°</td></tr>"
        "</table>")
        .arg(textPrimary, textDim)
        .arg(d.yaw, 0, 'f', 1)
        .arg(d.pitch, 0, 'f', 1);

    // ── Confidence ──
    float mean_maxp = (d.yaw_maxp + d.pitch_maxp) * 0.5f;
    html += QStringLiteral("<p style='margin-bottom:4px;'><b style='color:%1;'>预测置信度：</b></p>").arg(textPrimary);
    html += QStringLiteral("<table style='color:%1;font-size:12px;'>"
        "<tr><td>yaw max_prob：</td><td style='color:%2;text-align:right;'>%3</td></tr>"
        "<tr><td>pitch max_prob：</td><td style='color:%2;text-align:right;'>%4</td></tr>"
        "<tr><td>均值：</td><td style='color:%2;text-align:right;'>%5</td></tr>"
        "</table>")
        .arg(textPrimary, textDim)
        .arg(d.yaw_maxp, 0, 'f', 4)
        .arg(d.pitch_maxp, 0, 'f', 4)
        .arg(mean_maxp, 0, 'f', 4);

    text_display_->setHtml(html);
}

QString L2csDetailWidget::FormatGaze(float yaw, float pitch) {
    (void)yaw; (void)pitch;
    return {};
}

}  // namespace ui
}  // namespace omniface
