#pragma once

#include <QDialog>

namespace omniface {
namespace ui {

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

}  // namespace ui
}  // namespace omniface
