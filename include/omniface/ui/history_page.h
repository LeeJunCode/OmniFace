#pragma once

#include <set>
#include <string>

#include <QCheckBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace omniface::ui {

class HistoryPage : public QWidget {
    Q_OBJECT

public:
    explicit HistoryPage(QWidget* parent = nullptr);
    void refresh();

signals:
    void backRequested();
    void taskSelected(const std::string& dir_path);
    void taskDeleteRequested(const std::string& dir_path);

private:
    void onSelectAll();
    void onBatchDelete();
    void updateBatchButtons();

    QScrollArea* scroll_ = nullptr;
    QWidget* content_ = nullptr;

    QCheckBox* select_all_cb_ = nullptr;
    QPushButton* batch_del_btn_ = nullptr;
    QWidget* batch_bar_ = nullptr;

    std::set<std::string> selected_;
    int total_items_ = 0;
};

}  // namespace omniface::ui
