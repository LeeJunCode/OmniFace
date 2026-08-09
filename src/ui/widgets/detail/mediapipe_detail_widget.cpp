#include "omniface/ui/widgets/detail/mediapipe_detail_widget.h"
#include "omniface/ui/theme.h"

#include <cmath>
#include <QFile>
#include <QVBoxLayout>
#include <QTextStream>

namespace omniface {
namespace ui {

namespace {

QString PitchLabel(float p) {
    if (p < -25) return QStringLiteral("明显上扬");
    if (p < -10) return QStringLiteral("略微上扬");
    if (p > 25)  return QStringLiteral("明显下俯");
    if (p > 10)  return QStringLiteral("略微下俯");
    return {};
}

QString YawLabel(float y) {
    if (y < -25) return QStringLiteral("明显右转");
    if (y < -10) return QStringLiteral("略微右转");
    if (y > 25)  return QStringLiteral("明显左转");
    if (y > 10)  return QStringLiteral("略微左转");
    return {};
}

QString RollLabel(float r) {
    if (r < -15) return QStringLiteral("明显左倾");
    if (r < -6)  return QStringLiteral("略微左倾");
    if (r > 15)  return QStringLiteral("明显右倾");
    if (r > 6)   return QStringLiteral("略微右倾");
    return {};
}

} // anonymous namespace

MediaPipeDetailWidget::MediaPipeDetailWidget(QWidget* parent)
    : BaseDetailWidget(parent) {
    SetupUI();
}

void MediaPipeDetailWidget::SetupUI() {
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

void MediaPipeDetailWidget::LoadCsvData() {
    frames_.clear();
    frame_to_index_.clear();

    QFile file(csv_path_);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString header = in.readLine();
    // Detect format:
    //   v3: frame,timestamp,reproj_rms,nose_proj_err,pitch,yaw,roll,...
    //   v2: frame,timestamp,score,reproj_rms,nose_proj_err,pitch,yaw,roll,...
    //   v1: frame,timestamp,score,pitch,yaw,roll,...
    //   v0: frame,timestamp,pitch,yaw,roll,...
    QStringList header_cols = header.split(QStringLiteral(","));
    bool has_reproj_v3 = (header_cols.size() > 2 && header_cols[2].trimmed() == QStringLiteral("reproj_rms"));
    bool has_reproj_v2 = (header_cols.size() > 3 && header_cols[3].trimmed() == QStringLiteral("reproj_rms"));
    bool has_score  = !has_reproj_v3 && !has_reproj_v2 && (header_cols.size() > 2 && header_cols[2].trimmed() == QStringLiteral("score"));
    int pitch_idx, yaw_idx, roll_idx, min_cols, lm_offset;
    if (has_reproj_v3) {
        pitch_idx = 4; yaw_idx = 5; roll_idx = 6; min_cols = 7; lm_offset = 7;
    } else if (has_reproj_v2) {
        pitch_idx = 5; yaw_idx = 6; roll_idx = 7; min_cols = 8; lm_offset = 8;
    } else if (has_score) {
        pitch_idx = 3; yaw_idx = 4; roll_idx = 5; min_cols = 6; lm_offset = 6;
    } else {
        pitch_idx = 2; yaw_idx = 3; roll_idx = 4; min_cols = 5; lm_offset = 5;
    }

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList cols = line.split(QStringLiteral(","));
        if (cols.size() < min_cols) continue;
        MediaPipeFrameData d;
        d.frame_number = cols.value(0).toInt();
        d.pitch = cols.value(pitch_idx).toFloat();
        d.yaw   = cols.value(yaw_idx).toFloat();
        d.roll  = cols.value(roll_idx).toFloat();
        d.landmark_count = (cols.size() >= lm_offset + 468*2)
            ? 468 : ((cols.size() - lm_offset) / 2);
        frame_to_index_[d.frame_number] = static_cast<int>(frames_.size());
        frames_.push_back(d);
    }
}

int MediaPipeDetailWidget::TotalFrames() const {
    return static_cast<int>(frames_.size());
}

void MediaPipeDetailWidget::SetFrameGrades(const std::unordered_map<int, char>& grades) {
    frame_grades_ = grades;
}

void MediaPipeDetailWidget::UpdateForFrame(int videoFrameNumber) {
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

    QString html;
    html += QStringLiteral("<h3 style='color:%1;margin-bottom:10px;'>MediaPipe FaceMesh 分析</h3>")
        .arg(primary);

    // ── Head pose ──
    QString pLabel = PitchLabel(d.pitch);
    QString yLabel = YawLabel(d.yaw);
    QString rLabel = RollLabel(d.roll);

    html += QStringLiteral("<p style='margin-bottom:8px;'><b style='color:%1;'>头部姿态：</b>")
        .arg(textPrimary);
    QStringList poseParts;
    if (!pLabel.isEmpty()) poseParts << pLabel;
    if (!yLabel.isEmpty()) poseParts << yLabel;
    if (!rLabel.isEmpty()) poseParts << rLabel;
    if (poseParts.isEmpty()) {
        html += QStringLiteral("面部正面朝向镜头，姿态端正。");
    } else {
        html += QStringLiteral("面部") + poseParts.join(QStringLiteral("，"));
    }
    html += QStringLiteral(" <span style='color:%1;font-size:11px;'>"
        "(pitch: %2°, yaw: %3°, roll: %4°)</span></p>")
        .arg(textDim)
        .arg(d.pitch, 0, 'f', 1).arg(d.yaw, 0, 'f', 1).arg(d.roll, 0, 'f', 1);

    // Grade display
    auto git = frame_grades_.find(d.frame_number);
    if (git != frame_grades_.end()) {
        QString grade_color;
        QString grade_text;
        switch (git->second) {
        case 'S': grade_color = "#4CAF50"; grade_text = QStringLiteral("卓越"); break;
        case 'A': grade_color = "#8BC34A"; grade_text = QStringLiteral("优秀"); break;
        case 'B': grade_color = "#FFC107"; grade_text = QStringLiteral("良好"); break;
        case 'C': grade_color = "#FF9800"; grade_text = QStringLiteral("一般"); break;
        default:  grade_color = "#F44336"; grade_text = QStringLiteral("较差"); break;
        }
        html += QStringLiteral("<p style='margin-top:4px;'><b style='color:%1;'>质量等级: </b>"
            "<span style='color:%2;font-weight:bold;'>%3 (%4)</span></p>")
            .arg(textPrimary, grade_color).arg(git->second).arg(grade_text);
    }

    text_display_->setHtml(html);
}

QString MediaPipeDetailWidget::FormatHeadPose(float pitch, float yaw, float roll) {
    (void)pitch; (void)yaw; (void)roll;
    return {};
}

}  // namespace ui
}  // namespace omniface
