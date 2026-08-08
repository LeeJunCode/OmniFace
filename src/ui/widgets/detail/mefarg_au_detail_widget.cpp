#include "omniface/ui/widgets/detail/mefarg_au_detail_widget.h"
#include "omniface/ui/theme.h"

#include <QFile>
#include <QVBoxLayout>
#include <QTextStream>

namespace omniface {
namespace ui {

// Must match kAuNames order in mefarg_au_estimator.h exactly
const std::array<std::string, 41> MefargAuDetailWidget::au_names_ = {{
    "AU1  眉心提升", "AU2  眉梢提升", "AU4  眉毛下压",
    "AU5  上眼睑提升", "AU6  脸颊提升", "AU7  眼睑收紧",
    "AU9  鼻部皱起", "AU10 上唇提升", "AU11 鼻唇沟加深",
    "AU12 嘴角提升", "AU13 脸颊鼓起", "AU14 嘴角收紧",
    "AU15 嘴角下压", "AU16 下唇下压", "AU17 下巴提升",
    "AU18 嘴唇缩拢", "AU19 舌头露出", "AU20 嘴角拉伸",
    "AU22 嘴唇漏斗状", "AU23 嘴唇收紧", "AU24 嘴唇互压",
    "AU25 双唇分开", "AU26 下巴下降", "AU27 嘴巴拉伸",
    "AU32 咬嘴唇", "AU38 鼻孔扩张", "AU39 鼻孔收缩",
    "AUL1  左侧眉心提升", "AUR1  右侧眉心提升",
    "AUL2  左侧眉梢提升", "AUR2  右侧眉梢提升",
    "AUL4  左侧眉毛下压", "AUR4  右侧眉毛下压",
    "AUL6  左侧脸颊提升", "AUR6  右侧脸颊提升",
    "AUL10 左侧上唇提升", "AUR10 右侧上唇提升",
    "AUL12 左侧嘴角提升", "AUR12 右侧嘴角提升",
    "AUL14 左侧嘴角收紧", "AUR14 右侧嘴角收紧"
}};

MefargAuDetailWidget::MefargAuDetailWidget(QWidget* parent)
    : BaseDetailWidget(parent) {
    SetupUI();
}

void MefargAuDetailWidget::SetupUI() {
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
        "  border-radius: 4px; padding: 8px; font-size: 11px; }")
        .arg(tm.DeepBg(), tm.TextPrimary(), tm.Border()));
    root->addWidget(text_display_, 1);
}

void MefargAuDetailWidget::LoadCsvData() {
    frame_au_probs_.clear();
    frame_numbers_.clear();
    frame_to_index_.clear();

    QFile file(csv_path_);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.readLine(); // skip header
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList cols = line.split(QStringLiteral(","));
        // CSV: frame, timestamp, then 41 pairs of (prob, active) = 84 columns
        if (cols.size() < 2 + kNumAU * 2) continue;
        int frame_number = cols.value(0).toInt();
        std::array<float, kNumAU> au;
        for (int i = 0; i < kNumAU; ++i)
            au[i] = cols.value(i * 2 + 2).toFloat();
        frame_to_index_[frame_number] = static_cast<int>(frame_au_probs_.size());
        frame_numbers_.push_back(frame_number);
        frame_au_probs_.push_back(au);
    }
}

int MefargAuDetailWidget::TotalFrames() const {
    return static_cast<int>(frame_au_probs_.size());
}

void MefargAuDetailWidget::UpdateForFrame(int videoFrameNumber) {
    auto& tm = ThemeManager::Instance();
    auto primary = tm.Primary();
    auto textPrimary = tm.TextPrimary();
    auto textDim = tm.TextDim();
    auto warning = tm.Warning();

    auto it = frame_to_index_.find(videoFrameNumber);

    if (it == frame_to_index_.end()) {
        frame_no_label_->setText(QStringLiteral("当前帧: %1").arg(videoFrameNumber));
        text_display_->setText(QStringLiteral("<p style='color:%1;'>"
            "（当前帧未分析到目标人物数据）</p>").arg(textDim));
        return;
    }

    const auto& au = frame_au_probs_[it->second];

    frame_no_label_->setText(QStringLiteral("当前帧: %1").arg(videoFrameNumber));

    QString html;
    html += QStringLiteral("<h3 style='color:%1;margin-bottom:10px;'>MEFARG-AU 表情分析</h3>")
        .arg(primary);

    // List activated AUs (prob > 0.5)
    int activeCount = 0;
    for (int i = 0; i < kNumAU; ++i) {
        if (au[i] > 0.5f) {
            activeCount++;
            QString auName = QString::fromStdString(au_names_[i]).trimmed();
            html += QStringLiteral("<p style='color:%1;margin-left:12px;margin-bottom:2px;'>"
                "%2: <span style='color:%3;'>%4</span></p>")
                .arg(warning)
                .arg(auName).arg(textPrimary)
                .arg(au[i], 0, 'f', 3);
        }
    }

    if (activeCount == 0) {
        html += QStringLiteral("<p style='color:%1;'>"
            "当前帧未检测到激活的面部动作单元，面部呈自然放松状态。</p>").arg(textDim);
    }

    text_display_->setHtml(html);
}

QString MefargAuDetailWidget::FormatAU(int au_index, float probability) {
    (void)au_index; (void)probability;
    return {};
}

}  // namespace ui
}  // namespace omniface
