#include "omniface/ui/theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace omniface {
namespace ui {

ThemeManager& ThemeManager::Instance() {
    static ThemeManager instance;
    return instance;
}

void ThemeManager::SetTheme(Theme theme) {
    if (current_ == theme) return;
    current_ = theme;
    emit ThemeChanged();
}

// ── Structural ──────────────────────────────────────────────

QString ThemeManager::SurfaceBg() const {
    return current_ == Theme::Dark ? QStringLiteral("#1a1a1a") : QStringLiteral("#ffffff");
}

QString ThemeManager::DeepBg() const {
    return current_ == Theme::Dark ? QStringLiteral("#0d0d0d") : QStringLiteral("#f0f0f0");
}

QString ThemeManager::Border() const {
    return current_ == Theme::Dark ? QStringLiteral("#ffffff") : QStringLiteral("#000000");
}

QString ThemeManager::HoverBg() const {
    return current_ == Theme::Dark ? QStringLiteral("#333333") : QStringLiteral("#e0e0e0");
}

QString ThemeManager::SelectedBg() const {
    return current_ == Theme::Dark ? QStringLiteral("#444444") : QStringLiteral("#d0d0d0");
}

QString ThemeManager::AltRow() const {
    return current_ == Theme::Dark ? QStringLiteral("#151515") : QStringLiteral("#f8f8f8");
}

QString ThemeManager::SidebarBg() const {
    return current_ == Theme::Dark ? QStringLiteral("#111111") : QStringLiteral("#f0f0f0");
}

QString ThemeManager::DarkPanelBg() const {
    return current_ == Theme::Dark ? QStringLiteral("#0a0a0a") : QStringLiteral("#e8e8e8");
}

// ── Text ────────────────────────────────────────────────────

QString ThemeManager::TextPrimary() const {
    return current_ == Theme::Dark ? QStringLiteral("#ffffff") : QStringLiteral("#000000");
}

QString ThemeManager::TextMuted() const {
    return current_ == Theme::Dark ? QStringLiteral("#aaaaaa") : QStringLiteral("#555555");
}

QString ThemeManager::TextDim() const {
    return current_ == Theme::Dark ? QStringLiteral("#777777") : QStringLiteral("#888888");
}

QString ThemeManager::TextOnPrimary() const {
    return current_ == Theme::Dark ? QStringLiteral("#ffffff") : QStringLiteral("#ffffff");
}

// ── Accent / Interactive ────────────────────────────────────

QString ThemeManager::Primary() const {
    return current_ == Theme::Dark ? QStringLiteral("#3b82f6") : QStringLiteral("#2563eb");
}

QString ThemeManager::PrimaryHover() const {
    return current_ == Theme::Dark ? QStringLiteral("#2563eb") : QStringLiteral("#1d4ed8");
}

QString ThemeManager::AccentBlue() const {
    return current_ == Theme::Dark ? QStringLiteral("#60a5fa") : QStringLiteral("#3b82f6");
}

QString ThemeManager::AccentBlueHover() const {
    return current_ == Theme::Dark ? QStringLiteral("#1e4a6f") : QStringLiteral("#dbeafe");
}

// ── Semantic ────────────────────────────────────────────────

QString ThemeManager::Success() const {
    return current_ == Theme::Dark ? QStringLiteral("#10b981") : QStringLiteral("#059669");
}

QString ThemeManager::SuccessBg() const {
    return current_ == Theme::Dark ? QStringLiteral("#065f46") : QStringLiteral("#d1fae5");
}

QString ThemeManager::SuccessText() const {
    return current_ == Theme::Dark ? QStringLiteral("#6ee7b7") : QStringLiteral("#047857");
}

QString ThemeManager::SuccessHover() const {
    return current_ == Theme::Dark ? QStringLiteral("#047857") : QStringLiteral("#059669");
}

QString ThemeManager::Warning() const {
    return current_ == Theme::Dark ? QStringLiteral("#fbbf24") : QStringLiteral("#d97706");
}

QString ThemeManager::Error() const {
    return current_ == Theme::Dark ? QStringLiteral("#dc2626") : QStringLiteral("#b91c1c");
}

QString ThemeManager::ErrorBg() const {
    return current_ == Theme::Dark ? QStringLiteral("#b91c1c") : QStringLiteral("#fecaca");
}

QString ThemeManager::ErrorBright() const {
    return current_ == Theme::Dark ? QStringLiteral("#ef4444") : QStringLiteral("#dc2626");
}

QString ThemeManager::Purple() const {
    return current_ == Theme::Dark ? QStringLiteral("#8b5cf6") : QStringLiteral("#7c3aed");
}

QString ThemeManager::PurpleText() const {
    return current_ == Theme::Dark ? QStringLiteral("#a78bfa") : QStringLiteral("#6d28d9");
}

// ── Static ──────────────────────────────────────────────────

QString ThemeManager::DisplayBlack() const {
    return QStringLiteral("#000000");
}

QString ThemeManager::White() const {
    return QStringLiteral("#ffffff");
}

// ── QPalette helpers ────────────────────────────────────────

QPalette ThemeManager::CreatePalette() const {
    QPalette p;
    if (current_ == Theme::Dark) {
        p.setColor(QPalette::Window,          QColor(26,  26,  26));
        p.setColor(QPalette::WindowText,      QColor(255, 255, 255));
        p.setColor(QPalette::Base,            QColor(26,  26,  26));
        p.setColor(QPalette::AlternateBase,   QColor(34,  34,  34));
        p.setColor(QPalette::ToolTipBase,     QColor(34,  34,  34));
        p.setColor(QPalette::ToolTipText,     QColor(255, 255, 255));
        p.setColor(QPalette::Text,            QColor(255, 255, 255));
        p.setColor(QPalette::Button,          QColor(51,  51,  51));
        p.setColor(QPalette::ButtonText,      QColor(255, 255, 255));
        p.setColor(QPalette::BrightText,      Qt::red);
        p.setColor(QPalette::Link,            QColor(96,  165, 250));
        p.setColor(QPalette::Highlight,       QColor(59,  130, 246));
        p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        p.setColor(QPalette::Disabled, QPalette::Text,       QColor(119, 119, 119));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(119, 119, 119));
    } else {
        p.setColor(QPalette::Window,          QColor(255, 255, 255));
        p.setColor(QPalette::WindowText,      QColor(0,   0,   0));
        p.setColor(QPalette::Base,            QColor(255, 255, 255));
        p.setColor(QPalette::AlternateBase,   QColor(248, 248, 248));
        p.setColor(QPalette::ToolTipBase,     QColor(255, 255, 255));
        p.setColor(QPalette::ToolTipText,     QColor(0,   0,   0));
        p.setColor(QPalette::Text,            QColor(0,   0,   0));
        p.setColor(QPalette::Button,          QColor(240, 240, 240));
        p.setColor(QPalette::ButtonText,      QColor(0,   0,   0));
        p.setColor(QPalette::BrightText,      Qt::red);
        p.setColor(QPalette::Link,            QColor(37,  99,  235));
        p.setColor(QPalette::Highlight,       QColor(37,  99,  235));
        p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        p.setColor(QPalette::Disabled, QPalette::Text,       QColor(136, 136, 136));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(136, 136, 136));
    }
    return p;
}

QString ThemeManager::GlobalStyleSheet() const {
    return QString();
}

}  // namespace ui
}  // namespace omniface
