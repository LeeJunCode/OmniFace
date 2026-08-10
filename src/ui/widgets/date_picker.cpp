#include "omniface/ui/widgets/date_picker.h"
#include "omniface/ui/theme.h"

#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>

#include "omniface/ui/widgets/calendar_widget.h"

namespace omniface {
namespace ui {

DatePicker::DatePicker(QWidget* parent)
    : QWidget(parent), display_format_(QStringLiteral("yyyy-MM-dd")) {
    BuildUI();
    UpdateLineEdit();
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged,
            this, &DatePicker::ReapplyTheme);
}

void DatePicker::BuildUI() {
    auto& tm = ThemeManager::Instance();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    line_edit_ = new QLineEdit(this);
    line_edit_->setReadOnly(true);
    line_edit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    line_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-top-left-radius: 4px; border-bottom-left-radius: 4px;"
        "  border-right: none;"
        "  padding: 5px 6px; color: %3;"
        "}"
        "QLineEdit:focus { border-color: %4; }")
        .arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    layout->addWidget(line_edit_, 1);

    calendar_btn_ = new QPushButton(QStringLiteral("\u25BC"), this);
    calendar_btn_->setCursor(Qt::PointingHandCursor);
    calendar_btn_->setFixedWidth(22);
    calendar_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    calendar_btn_->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1; border: 1px solid %2;"
        "  border-top-right-radius: 4px; border-bottom-right-radius: 4px;"
        "  border-left: none;"
        "  color: %3; font-size: 8px; padding: 0; margin: 0;"
        "}"
        "QPushButton:hover { background: %4; }")
        .arg(tm.HoverBg(), tm.Border(), tm.TextMuted(), tm.TextDim()));
    connect(calendar_btn_, &QPushButton::clicked, this, &DatePicker::ShowPopup);
    layout->addWidget(calendar_btn_);

    line_edit_->installEventFilter(this);
}

bool DatePicker::eventFilter(QObject* obj, QEvent* event) {
    if (obj == line_edit_ && event->type() == QEvent::MouseButtonPress) {
        ShowPopup();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void DatePicker::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();
    line_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-top-left-radius: 4px; border-bottom-left-radius: 4px;"
        "  border-right: none;"
        "  padding: 5px 6px; color: %3;"
        "}"
        "QLineEdit:focus { border-color: %4; }")
        .arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    calendar_btn_->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1; border: 1px solid %2;"
        "  border-top-right-radius: 4px; border-bottom-right-radius: 4px;"
        "  border-left: none;"
        "  color: %3; font-size: 8px; padding: 0; margin: 0;"
        "}"
        "QPushButton:hover { background: %4; }")
        .arg(tm.HoverBg(), tm.Border(), tm.TextMuted(), tm.TextDim()));
}

void DatePicker::UpdateLineEdit() {
    if (!current_date_.isValid() && !special_value_text_.isEmpty()) {
        line_edit_->setText(special_value_text_);
    } else if (current_date_.isValid()) {
        line_edit_->setText(current_date_.toString(display_format_));
    } else {
        line_edit_->clear();
    }
}

void DatePicker::ShowPopup() {
    if (popup_) {
        HidePopup();
        return;
    }

    popup_ = new QWidget(nullptr, Qt::Popup);
    popup_->setAttribute(Qt::WA_DeleteOnClose);
    connect(popup_, &QWidget::destroyed, this, [this] {
        popup_ = nullptr;
        calendar_ = nullptr;
    });

    auto* popup_layout = new QHBoxLayout(popup_);
    popup_layout->setContentsMargins(0, 0, 0, 0);

    calendar_ = new CalendarWidget(popup_);
    if (current_date_.isValid()) {
        calendar_->setSelectedDate(current_date_);
    }
    if (min_date_.isValid()) calendar_->setMinimumDate(min_date_);
    if (max_date_.isValid()) calendar_->setMaximumDate(max_date_);

    connect(calendar_, &CalendarWidget::activated, this, &DatePicker::OnDateSelected);
    popup_layout->addWidget(calendar_);

    QPoint pos = mapToGlobal(QPoint(0, height() + 2));
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeom = screen->availableGeometry();
        QSize popupSize = popup_->sizeHint();
        if (pos.x() + popupSize.width() > screenGeom.right()) {
            pos.setX(screenGeom.right() - popupSize.width());
        }
        if (pos.y() + popupSize.height() > screenGeom.bottom()) {
            pos.setY(mapToGlobal(QPoint(0, 0)).y() - popupSize.height() - 2);
        }
    }
    popup_->move(pos);
    popup_->show();
}

void DatePicker::HidePopup() {
    if (popup_) {
        popup_->close();
        popup_ = nullptr;
        calendar_ = nullptr;
    }
}

void DatePicker::OnDateSelected(const QDate& date) {
    current_date_ = date;
    UpdateLineEdit();
    HidePopup();
    emit dateChanged(date);
}

QDate DatePicker::date() const {
    return current_date_;
}

void DatePicker::setDate(const QDate& date) {
    current_date_ = date;
    UpdateLineEdit();
}

QString DatePicker::displayFormat() const {
    return display_format_;
}

void DatePicker::setDisplayFormat(const QString& format) {
    display_format_ = format;
    UpdateLineEdit();
}

QString DatePicker::specialValueText() const {
    return special_value_text_;
}

void DatePicker::setSpecialValueText(const QString& text) {
    special_value_text_ = text;
    UpdateLineEdit();
}

void DatePicker::setCalendarPopup(bool enabled) {
    Q_UNUSED(enabled);
}

void DatePicker::setMinimumDate(const QDate& date) {
    min_date_ = date;
}

void DatePicker::setMaximumDate(const QDate& date) {
    max_date_ = date;
}

}  // namespace ui
}  // namespace omniface
