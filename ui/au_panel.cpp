#include "omniface/ui/au_panel.h"

#include <algorithm>
#include <map>

#include <QPainter>
#include <QPaintEvent>

namespace {

const std::map<std::string, std::string> kAUDescriptions = {
    {"AU01", "Inner Brow Raiser"},
    {"AU02", "Outer Brow Raiser"},
    {"AU04", "Brow Lowerer"},
    {"AU05", "Upper Lid Raiser"},
    {"AU06", "Cheek Raiser"},
    {"AU07", "Lid Tightener"},
    {"AU09", "Nose Wrinkler"},
    {"AU10", "Upper Lip Raiser"},
    {"AU12", "Lip Corner Puller"},
    {"AU14", "Dimpler"},
    {"AU15", "Lip Corner Depressor"},
    {"AU17", "Chin Raiser"},
    {"AU20", "Lip Stretcher"},
    {"AU23", "Lip Tightener"},
    {"AU25", "Lips Part"},
    {"AU26", "Jaw Drop"},
    {"AU28", "Lip Suck"},
    {"AU45", "Blink"},
};

constexpr int kBarHeight = 18;
constexpr int kBarSpacing = 4;
constexpr int kHeaderHeight = 32;
constexpr int kHPadding = 10;
constexpr int kVPadding = 8;
constexpr int kNameWidth = 48;
constexpr int kValueWidth = 44;
constexpr int kMinBarWidth = 80;
constexpr double kMaxIntensity = 5.0;

}  // namespace

namespace omniface::ui {

AUPanel::AUPanel(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(200);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void AUPanel::setActionUnits(
    const std::vector<std::pair<std::string, double>>& action_units) {
    au_entries_.clear();
    for (const auto& [code, intensity] : action_units) {
        std::string desc = code;
        auto it = kAUDescriptions.find(code);
        if (it != kAUDescriptions.end()) desc = it->second;
        au_entries_.push_back({code, desc, intensity});
    }
    update();
}

void AUPanel::clear() {
    au_entries_.clear();
    update();
}

QSize AUPanel::minimumSizeHint() const {
    return QSize(200, 100);
}

QSize AUPanel::sizeHint() const {
    int h = kHeaderHeight + kVPadding * 2 +
            static_cast<int>(au_entries_.size()) * (kBarHeight + kBarSpacing);
    return QSize(220, std::max(h, 100));
}

void AUPanel::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    // 进度条区域宽度 = 面板宽度 - 左右内边距 - AU代码列宽 - 数值列宽 - 间距
    int bar_area_w = w - kHPadding * 2 - kNameWidth - kValueWidth - 12;
    bar_area_w = std::max(bar_area_w, kMinBarWidth);

    p.fillRect(rect(), QColor(0x18, 0x18, 0x25));

    QFont title_font = font();
    title_font.setBold(true);
    title_font.setPointSize(title_font.pointSize() + 1);
    p.setFont(title_font);
    p.setPen(QColor(0x89, 0xb4, 0xfa));
    p.drawText(kHPadding, kVPadding, w - kHPadding * 2, kHeaderHeight,
               Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("Action Units"));

    if (au_entries_.empty()) {
        p.setPen(QColor(0x6c, 0x70, 0x86));
        QFont f = font();
        f.setPointSize(f.pointSize() - 1);
        p.setFont(f);
        p.drawText(QRect(0, kHeaderHeight + kVPadding, w, 40),
                   Qt::AlignCenter, QStringLiteral("No data"));
        return;
    }

    QFont au_font = font();
    au_font.setPointSize(std::max(au_font.pointSize() - 1, 7));
    p.setFont(au_font);

    int y = kHeaderHeight + kVPadding;

    for (const auto& au : au_entries_) {
        // 强度>0.1视为该AU被激活，以高亮文字和彩色进度条显示
        bool present = au.intensity > 0.1;
        QColor text_color = present ? QColor(0xcd, 0xd6, 0xf4) : QColor(0x6c, 0x70, 0x86);

        p.setPen(text_color);
        p.drawText(kHPadding, y, kNameWidth, kBarHeight,
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromStdString(au.code));

        int bar_x = kHPadding + kNameWidth + 4;
        QRect bar_bg(bar_x, y + 3, bar_area_w, kBarHeight - 6);
        p.fillRect(bar_bg, QColor(0x31, 0x32, 0x44));

        if (present) {
            double ratio = std::min(au.intensity / kMaxIntensity, 1.0);
            int fill_w = static_cast<int>(bar_area_w * ratio);
            if (fill_w > 0) {
                QRect bar_fill(bar_x, y + 3, fill_w, kBarHeight - 6);
                // 根据强度比例分四级着色：蓝(低) -> 绿(中低) -> 黄(中高) -> 红(高)
                QColor bar_color;
                if (ratio < 0.25)
                    bar_color = QColor(0x89, 0xb4, 0xfa);
                else if (ratio < 0.5)
                    bar_color = QColor(0xa6, 0xe3, 0xa1);
                else if (ratio < 0.75)
                    bar_color = QColor(0xf9, 0xe2, 0xaf);
                else
                    bar_color = QColor(0xfa, 0xb3, 0x87);
                p.fillRect(bar_fill, bar_color);
            }
        }

        QString val_text = present ? QString::number(au.intensity, 'f', 2) : QStringLiteral("0.00");
        p.setPen(text_color);
        p.drawText(bar_x + bar_area_w + 4, y, kValueWidth, kBarHeight,
                   Qt::AlignVCenter | Qt::AlignLeft, val_text);

        y += kBarHeight + kBarSpacing;
    }
}

}  // namespace omniface::ui
