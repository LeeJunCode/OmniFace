#pragma once

#include <QDate>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include <vector>

namespace omniface {
namespace ui {

class CalendarWidget : public QWidget {
    Q_OBJECT
public:
    explicit CalendarWidget(QWidget* parent = nullptr);

    QDate selectedDate() const;
    void setSelectedDate(const QDate& date);
    void setMinimumDate(const QDate& date);
    void setMaximumDate(const QDate& date);

signals:
    void clicked(const QDate& date);
    void activated(const QDate& date);

private:
    void BuildUI();
    void RefreshMonth();
    void GoToPrevMonth();
    void GoToNextMonth();
    void GoToPrevYear();
    void GoToNextYear();
    void OnDayClicked(int index);

    QLabel* month_year_label_ = nullptr;
    QPushButton* prev_year_btn_ = nullptr;
    QPushButton* prev_btn_ = nullptr;
    QPushButton* next_btn_ = nullptr;
    QPushButton* next_year_btn_ = nullptr;
    QGridLayout* day_grid_ = nullptr;

    QDate current_month_;
    QDate selected_date_;
    QDate min_date_;
    QDate max_date_;

    std::vector<QPushButton*> day_buttons_;
};

}  // namespace ui
}  // namespace omniface
