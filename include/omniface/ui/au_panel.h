#pragma once

#include <string>
#include <vector>
#include <utility>

#include <QLabel>
#include <QScrollArea>
#include <QWidget>

namespace omniface::ui {

class AUPanel : public QWidget {
    Q_OBJECT

public:
    explicit AUPanel(QWidget* parent = nullptr);

    void setActionUnits(
        const std::vector<std::pair<std::string, double>>& action_units);

    void clear();

private:
    void rebuildHtml();

    QScrollArea* scroll_area_ = nullptr;
    QLabel* content_label_ = nullptr;
    std::vector<std::pair<std::string, double>> action_units_;
};

}  // namespace omniface::ui
