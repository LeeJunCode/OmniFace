#include "omniface/ui/pages/home_page.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "omniface/ui/theme.h"

namespace omniface {
namespace ui {

HomePage::HomePage(std::shared_ptr<db::VideoRepo> video_repo,
                   std::shared_ptr<db::ImageRepo> image_repo,
                   std::shared_ptr<db::TaskRepo> task_repo,
                   QWidget* parent)
    : QWidget(parent)
    , video_repo_(std::move(video_repo))
    , image_repo_(std::move(image_repo))
    , task_repo_(std::move(task_repo)) {
    SetupUI();
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged,
            this, &HomePage::ReapplyTheme);
}

void HomePage::SetupUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(48, 40, 48, 40);
    root->setSpacing(24);

    auto& tm = ThemeManager::Instance();

    // ── Title ──────────────────────────────────────────────────
    auto* title = new QLabel(QStringLiteral("OmniFace"), this);
    title_label_ = title;
    title->setStyleSheet(
        QString("QLabel { color: %1; font-size: 32px; font-weight: bold; }")
            .arg(tm.TextPrimary()));
    root->addWidget(title);

    auto* subtitle = new QLabel(QStringLiteral("\u667A\u80FD\u4EBA\u8138\u5206\u6790\uFF0C\u4E00\u7AD9\u5B8C\u6210"), this);
    subtitle_label_ = subtitle;
    subtitle->setStyleSheet(
        QString("QLabel { color: %1; font-size: 15px; }").arg(tm.TextDim()));
    root->addWidget(subtitle);

    root->addSpacing(16);

    // ── Quick actions ──────────────────────────────────────────
    auto* actions_label = new QLabel(QStringLiteral("\u5FEB\u901F\u64CD\u4F5C"), this);
    actions_label_ = actions_label;
    actions_label->setStyleSheet(
        QString("QLabel { color: %1; font-size: 13px; font-weight: bold; }")
            .arg(tm.TextMuted()));
    root->addWidget(actions_label);

    auto* actions_layout = new QHBoxLayout();
    actions_layout->setSpacing(16);

    auto makeActionBtn = [this, &tm](const QString& text, const QString& icon, const QString& color) {
        auto* btn = new QPushButton(icon + QStringLiteral("  ") + text, this);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(56);
        btn->setMinimumWidth(160);
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background: %1; color: %2; border: 1px solid %3;"
            "  border-radius: 8px; font-size: 15px; font-weight: bold;"
            "}"
            "QPushButton:hover { background: %4; border-color: %2; }")
            .arg(tm.SurfaceBg(), color, tm.Border(), tm.HoverBg()));
        return btn;
    };

    auto* proc_btn = makeActionBtn(QStringLiteral("\u5F00\u59CB\u5904\u7406"), QStringLiteral("\u25B6"),
                                    tm.Primary());
    proc_btn_ = proc_btn;
    connect(proc_btn, &QPushButton::clicked, this, &HomePage::NavigateToProcessing);
    actions_layout->addWidget(proc_btn);

    auto* video_btn = makeActionBtn(QStringLiteral("\u89C6\u9891\u7BA1\u7406"), QStringLiteral("\U0001F3AC"),
                                     tm.AccentBlue());
    video_btn_ = video_btn;
    connect(video_btn, &QPushButton::clicked, this, &HomePage::NavigateToVideos);
    actions_layout->addWidget(video_btn);

    auto* image_btn = makeActionBtn(QStringLiteral("\u56FE\u7247\u7BA1\u7406"), QStringLiteral("\U0001F5BC"),
                                     tm.Success());
    image_btn_ = image_btn;
    connect(image_btn, &QPushButton::clicked, this, &HomePage::NavigateToImages);
    actions_layout->addWidget(image_btn);

    auto* history_btn = makeActionBtn(QStringLiteral("\u4EFB\u52A1\u5386\u53F2"), QStringLiteral("\U0001F4CB"),
                                       tm.PurpleText());
    history_btn_ = history_btn;
    connect(history_btn, &QPushButton::clicked, this, &HomePage::NavigateToHistory);
    actions_layout->addWidget(history_btn);

    actions_layout->addStretch();
    root->addLayout(actions_layout);

    root->addStretch();
}

void HomePage::Refresh() {
    // Page is static — no dynamic content to refresh
}

void HomePage::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();
    title_label_->setStyleSheet(
        QString("QLabel { color: %1; font-size: 32px; font-weight: bold; }")
            .arg(tm.TextPrimary()));
    subtitle_label_->setStyleSheet(
        QString("QLabel { color: %1; font-size: 15px; }").arg(tm.TextDim()));
    actions_label_->setStyleSheet(
        QString("QLabel { color: %1; font-size: 13px; font-weight: bold; }")
            .arg(tm.TextMuted()));

    auto styleBtn = [&](QPushButton* btn, const QString& color) {
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background: %1; color: %2; border: 1px solid %3;"
            "  border-radius: 8px; font-size: 15px; font-weight: bold;"
            "}"
            "QPushButton:hover { background: %4; border-color: %2; }")
            .arg(tm.SurfaceBg(), color, tm.Border(), tm.HoverBg()));
    };
    styleBtn(proc_btn_, tm.Primary());
    styleBtn(video_btn_, tm.AccentBlue());
    styleBtn(image_btn_, tm.Success());
    styleBtn(history_btn_, tm.PurpleText());
}

}  // namespace ui
}  // namespace omniface
