#include "omniface/ui/widgets/calendar_widget.h"
#include "omniface/ui/theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace omniface {
namespace ui {

CalendarWidget::CalendarWidget(QWidget* parent)
    : QWidget(parent), current_month_(QDate::currentDate().year(), QDate::currentDate().month(), 1) {
    auto& tm = ThemeManager::Instance();
    setStyleSheet(QStringLiteral("CalendarWidget { background: %1; }").arg(tm.SurfaceBg()));
    BuildUI();
    RefreshMonth();
}

void CalendarWidget::BuildUI() {
    auto& tm = ThemeManager::Instance();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── Header: << < month-year > >> ──────────────────────────
    auto* header = new QHBoxLayout();
    header->setSpacing(2);

    auto navBtnStyle = QStringLiteral(
        "QPushButton { color: %1; border: none; font-size: 11px;"
        "  min-width: 24px; min-height: 28px; }"
        "QPushButton:hover { color: %2; background: %3; border-radius: 4px; }")
        .arg(tm.TextMuted(), tm.TextPrimary(), tm.HoverBg());

    prev_year_btn_ = new QPushButton(QStringLiteral("\u00AB"), this);
    prev_year_btn_->setFlat(true);
    prev_year_btn_->setCursor(Qt::PointingHandCursor);
    prev_year_btn_->setToolTip(QStringLiteral("上一年"));
    prev_year_btn_->setStyleSheet(navBtnStyle);
    connect(prev_year_btn_, &QPushButton::clicked, this, &CalendarWidget::GoToPrevYear);
    header->addWidget(prev_year_btn_);

    prev_btn_ = new QPushButton(QStringLiteral("\u2039"), this);
    prev_btn_->setFlat(true);
    prev_btn_->setCursor(Qt::PointingHandCursor);
    prev_btn_->setToolTip(QStringLiteral("上一月"));
    prev_btn_->setStyleSheet(navBtnStyle);
    connect(prev_btn_, &QPushButton::clicked, this, &CalendarWidget::GoToPrevMonth);
    header->addWidget(prev_btn_);

    month_year_label_ = new QLabel(this);
    month_year_label_->setAlignment(Qt::AlignCenter);
    month_year_label_->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 14px; font-weight: bold; }")
        .arg(tm.TextPrimary()));
    header->addWidget(month_year_label_, 1);

    next_btn_ = new QPushButton(QStringLiteral("\u203A"), this);
    next_btn_->setFlat(true);
    next_btn_->setCursor(Qt::PointingHandCursor);
    next_btn_->setToolTip(QStringLiteral("下一月"));
    next_btn_->setStyleSheet(navBtnStyle);
    connect(next_btn_, &QPushButton::clicked, this, &CalendarWidget::GoToNextMonth);
    header->addWidget(next_btn_);

    next_year_btn_ = new QPushButton(QStringLiteral("\u00BB"), this);
    next_year_btn_->setFlat(true);
    next_year_btn_->setCursor(Qt::PointingHandCursor);
    next_year_btn_->setToolTip(QStringLiteral("下一年"));
    next_year_btn_->setStyleSheet(navBtnStyle);
    connect(next_year_btn_, &QPushButton::clicked, this, &CalendarWidget::GoToNextYear);
    header->addWidget(next_year_btn_);

    root->addLayout(header);

    // ── Weekday labels ─────────────────────────────────────────
    static const char* kWeekdayNames[] = {"一", "二", "三", "四", "五", "六", "日"};
    auto* wday_layout = new QHBoxLayout();
    wday_layout->setSpacing(0);
    for (int i = 0; i < 7; ++i) {
        auto* lbl = new QLabel(QString::fromUtf8(kWeekdayNames[i]), this);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 12px; min-width: 32px; }")
            .arg(tm.TextDim()));
        wday_layout->addWidget(lbl);
    }
    root->addLayout(wday_layout);

    // ── 6×7 day grid ───────────────────────────────────────────
    day_grid_ = new QGridLayout();
    day_grid_->setSpacing(2);
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            auto* btn = new QPushButton(this);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            int index = row * 7 + col;
            connect(btn, &QPushButton::clicked, this, [this, index] { OnDayClicked(index); });
            day_grid_->addWidget(btn, row, col);
            day_buttons_.push_back(btn);
        }
    }
    root->addLayout(day_grid_);
}

void CalendarWidget::RefreshMonth() {
    auto& tm = ThemeManager::Instance();

    month_year_label_->setText(current_month_.toString(QStringLiteral("yyyy 年 M 月")));

    // Build day button stylesheets
    auto dayStyle = QStringLiteral(
        "QPushButton {"
        "  background: transparent; border: none; border-radius: 4px;"
        "  color: %1; font-size: 13px; min-width: 32px; min-height: 28px;"
        "}"
        "QPushButton:hover { background: %2; }")
        .arg(tm.TextPrimary(), tm.HoverBg());

    auto todayStyle = QStringLiteral(
        "QPushButton {"
        "  background: transparent; border: 1px solid %1; border-radius: 4px;"
        "  color: %2; font-size: 13px; min-width: 32px; min-height: 28px;"
        "}"
        "QPushButton:hover { background: %3; }")
        .arg(tm.Primary(), tm.TextPrimary(), tm.HoverBg());

    auto selectedStyle = QStringLiteral(
        "QPushButton {"
        "  background: %1; border: none; border-radius: 4px;"
        "  color: %2; font-size: 13px; font-weight: bold;"
        "  min-width: 32px; min-height: 28px;"
        "}"
        "QPushButton:hover { background: %3; }")
        .arg(tm.Primary(), tm.White(), tm.PrimaryHover());

    auto dimmedStyle = QStringLiteral(
        "QPushButton {"
        "  background: transparent; border: none; border-radius: 4px;"
        "  color: %1; font-size: 13px; min-width: 32px; min-height: 28px;"
        "}"
        "QPushButton:hover { background: %2; }")
        .arg(tm.TextDim(), tm.HoverBg());

    auto disabledStyle = QStringLiteral(
        "QPushButton {"
        "  background: transparent; border: none; border-radius: 4px;"
        "  color: %1; font-size: 13px; min-width: 32px; min-height: 28px;"
        "}")
        .arg(tm.TextDim());

    QDate firstOfMonth(current_month_.year(), current_month_.month(), 1);
    int startDow = firstOfMonth.dayOfWeek();  // 1=Mon … 7=Sun
    int startCol = startDow - 1;               // 0-based: Mon=0 … Sun=6
    int daysInMonth = firstOfMonth.daysInMonth();
    QDate today = QDate::currentDate();

    for (int i = 0; i < 42; ++i) {
        QPushButton* btn = day_buttons_[i];
        int col = i % 7;
        int row = i / 7;

        // Determine which date this cell represents
        QDate cellDate;
        bool thisMonth = false;
        if (i < startCol) {
            // Preceding month
            int day = firstOfMonth.addDays(-(startCol - i)).day();
            cellDate = QDate(current_month_.year(), current_month_.month(), 1).addDays(-(startCol - i));
            btn->setText(QString::number(day));
        } else if (i >= startCol + daysInMonth) {
            // Following month
            int day = i - startCol - daysInMonth + 1;
            cellDate = QDate(current_month_.year(), current_month_.month(), daysInMonth).addDays(i - startCol - daysInMonth + 1);
            btn->setText(QString::number(day));
        } else {
            // Current month
            int day = i - startCol + 1;
            cellDate = QDate(current_month_.year(), current_month_.month(), day);
            btn->setText(QString::number(day));
            thisMonth = true;
        }

        btn->setEnabled(true);
        btn->setVisible(true);

        // Check date range
        bool outOfRange = false;
        if (min_date_.isValid() && cellDate < min_date_) outOfRange = true;
        if (max_date_.isValid() && cellDate > max_date_) outOfRange = true;

        if (outOfRange) {
            btn->setStyleSheet(disabledStyle);
            btn->setEnabled(false);
        } else if (selected_date_.isValid() && cellDate == selected_date_) {
            btn->setStyleSheet(selectedStyle);
        } else if (!thisMonth) {
            btn->setStyleSheet(dimmedStyle);
        } else if (cellDate == today) {
            btn->setStyleSheet(todayStyle);
        } else {
            btn->setStyleSheet(dayStyle);
        }
    }
}

void CalendarWidget::GoToPrevMonth() {
    current_month_ = current_month_.addMonths(-1);
    RefreshMonth();
}

void CalendarWidget::GoToNextMonth() {
    current_month_ = current_month_.addMonths(1);
    RefreshMonth();
}

void CalendarWidget::GoToPrevYear() {
    current_month_ = current_month_.addYears(-1);
    RefreshMonth();
}

void CalendarWidget::GoToNextYear() {
    current_month_ = current_month_.addYears(1);
    RefreshMonth();
}

void CalendarWidget::OnDayClicked(int index) {
    if (!day_buttons_[index]->isEnabled()) return;

    QDate firstOfMonth(current_month_.year(), current_month_.month(), 1);
    int startCol = firstOfMonth.dayOfWeek() - 1;
    int daysInMonth = firstOfMonth.daysInMonth();

    QDate cellDate;
    if (index < startCol) {
        // Preceding month — navigate there first
        current_month_ = current_month_.addMonths(-1);
        int day = firstOfMonth.addDays(-(startCol - index)).day();
        selected_date_ = QDate(current_month_.year(), current_month_.month(), day);
        emit clicked(selected_date_);
        emit activated(selected_date_);
        RefreshMonth();
    } else if (index >= startCol + daysInMonth) {
        // Following month — navigate there first
        current_month_ = current_month_.addMonths(1);
        int day = index - startCol - daysInMonth + 1;
        selected_date_ = QDate(current_month_.year(), current_month_.month(), day);
        emit clicked(selected_date_);
        emit activated(selected_date_);
        RefreshMonth();
    } else {
        // Current month
        int day = index - startCol + 1;
        selected_date_ = QDate(current_month_.year(), current_month_.month(), day);
        emit clicked(selected_date_);
        emit activated(selected_date_);
        RefreshMonth();
    }
}

QDate CalendarWidget::selectedDate() const {
    return selected_date_;
}

void CalendarWidget::setSelectedDate(const QDate& date) {
    selected_date_ = date;
    if (date.isValid()) {
        current_month_ = QDate(date.year(), date.month(), 1);
    }
    RefreshMonth();
}

void CalendarWidget::setMinimumDate(const QDate& date) {
    min_date_ = date;
    RefreshMonth();
}

void CalendarWidget::setMaximumDate(const QDate& date) {
    max_date_ = date;
    RefreshMonth();
}

}  // namespace ui
}  // namespace omniface
