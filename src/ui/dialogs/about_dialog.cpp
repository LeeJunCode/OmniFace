#include "omniface/ui/dialogs/about_dialog.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "omniface/ui/theme.h"

namespace omniface {
namespace ui {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent) {
    auto& tm = ThemeManager::Instance();
    setWindowTitle(QStringLiteral("关于 OmniFace"));
    setFixedSize(380, 220);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QLabel { color: %2; }").arg(tm.SurfaceBg(), tm.TextPrimary()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(32, 28, 32, 24);
    root->setAlignment(Qt::AlignCenter);
    root->setSpacing(6);

    auto* title = new QLabel(QStringLiteral("OmniFace"), this);
    title->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 26px; font-weight: bold; color: %1; }").arg(tm.Primary()));
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* version = new QLabel(QStringLiteral("版本 1.0.0"), this);
    version->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 13px; color: %1; }").arg(tm.TextMuted()));
    version->setAlignment(Qt::AlignCenter);
    root->addWidget(version);

    root->addSpacing(14);

    auto* desc = new QLabel(QStringLiteral(
        "实时人脸检测、跟踪与识别桌面应用"), this);
    desc->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 12px; color: %1; }").arg(tm.TextMuted()));
    desc->setAlignment(Qt::AlignCenter);
    root->addWidget(desc);

    root->addSpacing(14);

    auto* copyright = new QLabel(QStringLiteral("Copyright © 2025 OmniFace"), this);
    copyright->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 11px; color: %1; }").arg(tm.TextDim()));
    copyright->setAlignment(Qt::AlignCenter);
    root->addWidget(copyright);

    root->addSpacing(14);

    auto* close_btn = new QPushButton(QStringLiteral("确定"), this);
    close_btn->setFixedWidth(100);
    close_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %3; border: none;"
        "  border-radius: 4px; padding: 8px 20px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }").arg(tm.Primary(), tm.PrimaryHover(), tm.White()));
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(close_btn, 0, Qt::AlignCenter);
}

}  // namespace ui
}  // namespace omniface
