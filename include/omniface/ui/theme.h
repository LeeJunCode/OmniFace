#pragma once

#include <QObject>
#include <QString>

namespace omniface {
namespace ui {

enum class Theme { Dark, Light };

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& Instance();

    Theme Current() const { return current_; }
    void SetTheme(Theme theme);

    // ── Structural ──────────────────────────────────────────
    QString SurfaceBg() const;       // Dark: #1f2937  Light: #ffffff
    QString DeepBg() const;          // Dark: #111827  Light: #f3f4f6
    QString Border() const;          // Dark: #374151  Light: #d1d5db
    QString HoverBg() const;         // Dark: #374151  Light: #e5e7eb
    QString SelectedBg() const;      // Dark: #1e3a5f  Light: #dbeafe
    QString AltRow() const;          // Dark: #1a1f2e  Light: #f9fafb
    QString SidebarBg() const;       // Dark: #1f1f1f  Light: #f3f4f6
    QString DarkPanelBg() const;     // Dark: #0f172a  Light: #e2e8f0

    // ── Text ────────────────────────────────────────────────
    QString TextPrimary() const;     // Dark: #e5e7eb  Light: #111827
    QString TextMuted() const;       // Dark: #9ca3af  Light: #6b7280
    QString TextDim() const;         // Dark: #6b7280  Light: #9ca3af
    QString TextOnPrimary() const;   // Dark: #93c5fd  Light: #1e40af

    // ── Accent / Interactive ────────────────────────────────
    QString Primary() const;         // Dark: #3b82f6  Light: #2563eb
    QString PrimaryHover() const;    // Dark: #2563eb  Light: #1d4ed8
    QString AccentBlue() const;      // Dark: #60a5fa  Light: #3b82f6
    QString AccentBlueHover() const; // Dark: #1e4a6f  Light: #dbeafe

    // ── Semantic ────────────────────────────────────────────
    QString Success() const;         // Dark: #10b981  Light: #059669
    QString SuccessBg() const;       // Dark: #065f46  Light: #d1fae5
    QString SuccessText() const;     // Dark: #6ee7b7  Light: #047857
    QString SuccessHover() const;    // Dark: #047857  Light: #059669
    QString Warning() const;         // Dark: #fbbf24  Light: #d97706
    QString Error() const;           // Dark: #dc2626  Light: #b91c1c
    QString ErrorBg() const;         // Dark: #b91c1c  Light: #fecaca
    QString ErrorBright() const;     // Dark: #ef4444  Light: #dc2626
    QString Purple() const;          // Dark: #8b5cf6  Light: #7c3aed
    QString PurpleText() const;      // Dark: #a78bfa  Light: #6d28d9

    // ── Static ──────────────────────────────────────────────
    QString DisplayBlack() const;    // #0a0a0a (both themes)
    QString White() const;           // #ffffff (both themes)

    // ── QPalette helpers ────────────────────────────────────
    QPalette CreatePalette() const;
    QString GlobalStyleSheet() const;

signals:
    void ThemeChanged();

private:
    ThemeManager() = default;
    Theme current_ = Theme::Light;
};

}  // namespace ui
}  // namespace omniface
