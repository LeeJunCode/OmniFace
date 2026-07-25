#pragma once

#include <string>
#include <vector>
#include <utility>

#include <QWidget>

namespace omniface::ui {

class AUPanel : public QWidget {
    Q_OBJECT

public:
    explicit AUPanel(QWidget* parent = nullptr);

    void setActionUnits(
        const std::vector<std::pair<std::string, double>>& action_units);

    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

private:
    struct AUEntry {
        std::string code;
        std::string description;
        double intensity;
    };
    std::vector<AUEntry> au_entries_;
    QString title_;
};

}  // namespace omniface::ui
