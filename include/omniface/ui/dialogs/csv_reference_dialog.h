#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QTextEdit>

namespace omniface {
namespace ui {

class CsvReferenceDialog : public QDialog {
    Q_OBJECT
public:
    explicit CsvReferenceDialog(QWidget* parent = nullptr);

    void SelectBackend(const QString& backend);

private slots:
    void OnTabChanged(int index);
    void OnBackendChanged(int row);

private:
    void SetupUI();
    void BuildContent();
    QListWidget* backend_list_;
    QStackedWidget* field_stack_;
    QStackedWidget* mapping_stack_;
    QStackedWidget* main_stack_;
};

}  // namespace ui
}  // namespace omniface
