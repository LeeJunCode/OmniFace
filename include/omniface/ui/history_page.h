#pragma once

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
    QScrollArea* scroll_ = nullptr;
    QWidget* content_ = nullptr;
};

}  // namespace omniface::ui
