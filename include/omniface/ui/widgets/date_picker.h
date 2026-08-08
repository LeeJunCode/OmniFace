#pragma once

#include <QDate>
#include <QEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace omniface {
namespace ui {

class CalendarWidget;

class DatePicker : public QWidget {
    Q_OBJECT
public:
    explicit DatePicker(QWidget* parent = nullptr);

    QDate date() const;
    void setDate(const QDate& date);

    QString displayFormat() const;
    void setDisplayFormat(const QString& format);

    QString specialValueText() const;
    void setSpecialValueText(const QString& text);

    void setCalendarPopup(bool enabled);
    void setMinimumDate(const QDate& date);
    void setMaximumDate(const QDate& date);

    QDate minimumDate() const { return min_date_; }

signals:
    void dateChanged(const QDate& date);

private:
    QLineEdit* line_edit_ = nullptr;
    QPushButton* calendar_btn_ = nullptr;
    CalendarWidget* calendar_ = nullptr;
    QWidget* popup_ = nullptr;

    QDate current_date_;
    QDate min_date_;
    QDate max_date_;
    QString display_format_;
    QString special_value_text_;

    void BuildUI();
    void ReapplyTheme();
    void UpdateLineEdit();
    void ShowPopup();
    void HidePopup();

private slots:
    void OnDateSelected(const QDate& date);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};

}  // namespace ui
}  // namespace omniface
