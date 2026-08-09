#include "omniface/ui/widgets/detail/openface_detail_widget.h"
#include "omniface/ui/theme.h"

#include <QFile>
#include <QVBoxLayout>
#include <QTextStream>

#include <cmath>
#include <unordered_map>

namespace omniface {
namespace ui {

namespace {

QString PitchLabel(float p) {
    if (p < -0.44f) return QStringLiteral("明显上扬");
    if (p < -0.17f) return QStringLiteral("略微上扬");
    if (p > 0.44f)  return QStringLiteral("明显下俯");
    if (p > 0.17f)  return QStringLiteral("略微下俯");
    return {};
}

QString YawLabel(float y) {
    if (y < -0.44f) return QStringLiteral("明显右转");
    if (y < -0.17f) return QStringLiteral("略微右转");
    if (y > 0.44f)  return QStringLiteral("明显左转");
    if (y > 0.17f)  return QStringLiteral("略微左转");
    return {};
}

QString RollLabel(float r) {
    if (r < -0.26f) return QStringLiteral("明显左倾");
    if (r < -0.10f) return QStringLiteral("略微左倾");
    if (r > 0.26f)  return QStringLiteral("明显右倾");
    if (r > 0.10f)  return QStringLiteral("略微右倾");
    return {};
}

QString GazeXLabel(float x) {
    if (x < -0.3f) return QStringLiteral("明显向右");
    if (x < -0.1f) return QStringLiteral("略微向右");
    if (x > 0.3f)  return QStringLiteral("明显向左");
    if (x > 0.1f)  return QStringLiteral("略微向左");
    return {};
}

QString GazeYLabel(float y) {
    if (y > 0.3f)  return QStringLiteral("明显向下");
    if (y > 0.1f)  return QStringLiteral("略微向下");
    if (y < -0.3f) return QStringLiteral("明显向上");
    if (y < -0.1f) return QStringLiteral("略微向上");
    return {};
}

QString AuName(int auNum) {
    static const std::unordered_map<int, QString> map = {
        {1, QStringLiteral("眉心提升")},   {2, QStringLiteral("眉梢提升")},
        {4, QStringLiteral("眉毛下压")},   {5, QStringLiteral("上眼睑提升")},
        {6, QStringLiteral("脸颊提升")},   {7, QStringLiteral("眼睑收紧")},
        {9, QStringLiteral("鼻部皱起")},   {10, QStringLiteral("上唇提升")},
        {12, QStringLiteral("嘴角提升")},  {14, QStringLiteral("嘴角收紧")},
        {15, QStringLiteral("嘴角下压")},  {17, QStringLiteral("下巴提升")},
        {20, QStringLiteral("嘴角拉伸")},  {23, QStringLiteral("嘴唇收紧")},
        {25, QStringLiteral("双唇分开")},  {26, QStringLiteral("下巴下降")},
        {28, QStringLiteral("嘴巴拉伸")},  {45, QStringLiteral("眨眼")},
    };
    auto it = map.find(auNum);
    return (it != map.end()) ? it->second : QStringLiteral("AU%1").arg(auNum);
}

} // anonymous namespace

OpenFaceDetailWidget::OpenFaceDetailWidget(QWidget* parent)
    : BaseDetailWidget(parent) {
    SetupUI();
}

void OpenFaceDetailWidget::SetupUI() {
    auto& tm = ThemeManager::Instance();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    face_image_label_ = new QLabel(this);
    face_image_label_->setAlignment(Qt::AlignCenter);
    face_image_label_->setMinimumHeight(160);
    face_image_label_->setMaximumHeight(240);
    face_image_label_->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 4px; }")
        .arg(tm.DarkPanelBg(), tm.Border()));
    face_image_label_->setText(QStringLiteral("（对齐人脸）"));
    root->addWidget(face_image_label_);

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

std::unordered_map<std::string, int> OpenFaceDetailWidget::BuildColumnMap(
    const QStringList& header_cols) {
    std::unordered_map<std::string, int> m;
    for (int i = 0; i < header_cols.size(); ++i) {
        m[header_cols[i].trimmed().toStdString()] = i;
    }
    return m;
}

OpenFaceFrameData OpenFaceDetailWidget::ParseRow(
    const QStringList& cols,
    const std::unordered_map<std::string, int>& cm) {

    OpenFaceFrameData d;

    auto get = [&](const char* name) -> float {
        auto it = cm.find(name);
        if (it != cm.end() && it->second < cols.size())
            return cols[it->second].toFloat();
        return 0.0f;
    };

    d.frame_number = static_cast<int>(get("frame"));
    d.confidence   = get("confidence");
    d.pose_rx  = get("pose_Rx");
    d.pose_ry  = get("pose_Ry");
    d.pose_rz  = get("pose_Rz");
    d.pose_tx  = get("pose_Tx");
    d.pose_ty  = get("pose_Ty");
    d.pose_tz  = get("pose_Tz");
    d.gaze_0_x = get("gaze_0_x");
    d.gaze_0_y = get("gaze_0_y");
    d.gaze_0_z = get("gaze_0_z");
    d.gaze_1_x = get("gaze_1_x");
    d.gaze_1_y = get("gaze_1_y");
    d.gaze_1_z = get("gaze_1_z");
    d.gaze_angle_x = get("gaze_angle_x");
    d.gaze_angle_y = get("gaze_angle_y");

    d.valid = (d.confidence > 0.0f || d.pose_rx != 0.0f || d.pose_ry != 0.0f ||
               d.gaze_angle_x != 0.0f || d.gaze_angle_y != 0.0f);

    // Parse AU columns: AU{n}_c == 1 means activated, AU{n}_r is intensity
    for (int auNum = 1; auNum <= 45; ++auNum) {
        std::string cKey = "AU" + std::string(auNum < 10 ? "0" : "") + std::to_string(auNum) + "_c";
        auto cIt = cm.find(cKey);
        if (cIt != cm.end() && cIt->second < cols.size()) {
            if (static_cast<int>(cols[cIt->second].toFloat()) == 1) {
                std::string rKey = "AU" + std::string(auNum < 10 ? "0" : "") + std::to_string(auNum) + "_r";
                auto rIt = cm.find(rKey);
                float intensity = (rIt != cm.end() && rIt->second < cols.size())
                    ? cols[rIt->second].toFloat() : 0.0f;
                d.activated_aus.emplace_back(auNum, intensity);
            }
        }
    }

    return d;
}

void OpenFaceDetailWidget::LoadCsvData() {
    frames_.clear();
    frame_to_index_.clear();

    QFile file(csv_path_);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString header_line = in.readLine();
    if (header_line.isEmpty()) return;

    QStringList header_cols = header_line.split(QStringLiteral(","));
    auto col_map = BuildColumnMap(header_cols);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList cols = line.split(QStringLiteral(","));
        auto d = ParseRow(cols, col_map);
        frame_to_index_[d.frame_number] = static_cast<int>(frames_.size());
        frames_.push_back(d);
    }
}

int OpenFaceDetailWidget::TotalFrames() const {
    return static_cast<int>(frames_.size());
}

void OpenFaceDetailWidget::SetFrameGrades(
    const std::unordered_map<int, char>& grades) {
    frame_grades_ = grades;
}

void OpenFaceDetailWidget::UpdateForFrame(int videoFrameNumber) {
    auto& tm = ThemeManager::Instance();
    auto primary = tm.Primary();
    auto textPrimary = tm.TextPrimary();
    auto textDim = tm.TextDim();
    auto warning = tm.Warning();

    auto it = frame_to_index_.find(videoFrameNumber);

    if (it == frame_to_index_.end()) {
        face_image_label_->setText(QStringLiteral("（无对齐人脸数据）"));
        frame_no_label_->setText(QStringLiteral("当前帧: %1").arg(videoFrameNumber));
        text_display_->setText(QStringLiteral("<p style='color:%1;'>"
            "（当前帧未分析到目标人物数据）</p>").arg(textDim));
        return;
    }

    const auto& d = frames_[it->second];
    frame_no_label_->setText(QStringLiteral("当前帧: %1").arg(videoFrameNumber - 1));

    // ── Aligned face image ──
    if (!aligned_face_dir_.isEmpty()) {
        QString imgPath = QStringLiteral("%1/frame_det_00_%2.bmp")
            .arg(aligned_face_dir_)
            .arg(d.frame_number, 6, 10, QChar('0'));
        QPixmap pix(imgPath);
        if (!pix.isNull()) {
            pix = pix.scaled(face_image_label_->width(), face_image_label_->maximumHeight(),
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
            face_image_label_->setPixmap(pix);
        } else {
            face_image_label_->setText(QStringLiteral("（对齐人脸图片未找到）"));
        }
    } else {
        face_image_label_->setText(QStringLiteral("（对齐人脸数据不可用）"));
    }

    if (!d.valid) {
        text_display_->setText(QStringLiteral("<p style='color:%1;'>本帧未检测到有效人脸数据。</p>")
            .arg(textDim));
        return;
    }

    QString html;
    html += QStringLiteral("<h3 style='color:%1;margin-bottom:6px;'>OpenFace 面部分析</h3>")
        .arg(primary);

    // ── Quality grade badge ──
    auto git = frame_grades_.find(d.frame_number);
    if (git != frame_grades_.end()) {
        char g = git->second;
        QColor gc;
        QString gLabel;
        switch (g) {
        case 'S': gc = QColor(0x21, 0x96, 0xF3); gLabel = QStringLiteral("S 卓越"); break;
        case 'A': gc = QColor(0x4C, 0xAF, 0x50); gLabel = QStringLiteral("A 优秀"); break;
        case 'B': gc = QColor(0xFF, 0x98, 0x00); gLabel = QStringLiteral("B 良好"); break;
        case 'C': gc = QColor(0xFF, 0x57, 0x22); gLabel = QStringLiteral("C 一般"); break;
        default:  gc = QColor(0xF4, 0x43, 0x36); gLabel = QStringLiteral("D 较差"); break;
        }
        html += QStringLiteral(
            "<p style='margin-bottom:10px;'>"
            "<span style='display:inline-block;background:%1;color:#fff;"
            "  border-radius:4px;padding:2px 10px;font-weight:bold;font-size:13px;'>"
            "%2</span></p>")
            .arg(gc.name(), gLabel);
    }

    // ── Head pose ──
    QString pLabel = PitchLabel(d.pose_rx);
    QString yLabel = YawLabel(d.pose_ry);
    QString rLabel = RollLabel(d.pose_rz);

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
        "(pitch: %2 rad, yaw: %3 rad, roll: %4 rad)</span></p>")
        .arg(textDim)
        .arg(d.pose_rx, 0, 'f', 1).arg(d.pose_ry, 0, 'f', 1).arg(d.pose_rz, 0, 'f', 1);

    // ── Gaze ──
    QString gxLabel = GazeXLabel(d.gaze_angle_x);
    QString gyLabel = GazeYLabel(d.gaze_angle_y);

    html += QStringLiteral("<p style='margin-bottom:8px;'><b style='color:%1;'>视线方向：</b>")
        .arg(textPrimary);
    if (gxLabel.isEmpty() && gyLabel.isEmpty()) {
        html += QStringLiteral("视线正视前方。");
    } else {
        QStringList gazeParts;
        if (!gyLabel.isEmpty()) gazeParts << gyLabel;
        if (!gxLabel.isEmpty()) gazeParts << gxLabel;
        html += QStringLiteral("视线") + gazeParts.join(QStringLiteral("、"));
    }
    html += QStringLiteral(" <span style='color:%1;font-size:11px;'>"
        "(gaze_x: %2 rad, gaze_y: %3 rad)</span></p>")
        .arg(textDim)
        .arg(d.gaze_angle_x, 0, 'f', 3).arg(d.gaze_angle_y, 0, 'f', 3);

    // ── Activated AUs ──
    if (!d.activated_aus.empty()) {
        html += QStringLiteral("<p style='margin-bottom:4px;'><b style='color:%1;'>激活的面部动作单元 (AU)：</b></p>")
            .arg(textPrimary);
        for (const auto& [auNum, intensity] : d.activated_aus) {
            html += QStringLiteral("<p style='color:%1;margin-left:12px;margin-bottom:2px;'>"
                "AU%2 %3: <span style='color:%4;'>%5</span></p>")
                .arg(warning)
                .arg(auNum, 2, 10, QChar('0')).arg(AuName(auNum))
                .arg(textPrimary)
                .arg(intensity, 0, 'f', 1);
        }
    } else {
        html += QStringLiteral("<p style='margin-bottom:8px;color:%1;'>"
            "本帧未检测到激活的面部动作单元。</p>").arg(textDim);
    }

    text_display_->setHtml(html);
}

}  // namespace ui
}  // namespace omniface
