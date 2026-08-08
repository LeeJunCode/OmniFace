#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>

#include "omniface/ui/widgets/date_picker.h"

#include <memory>

namespace omniface {
namespace db {
class VideoRepo;
struct VideoRecord;
}

namespace ui {

class VideoTableModel;
class VideoFilterProxy;

class VideoSelectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit VideoSelectionDialog(std::shared_ptr<db::VideoRepo> repo, QWidget* parent = nullptr);

    int64_t SelectedVideoId() const { return selected_id_; }
    QString SelectedVideoName() const { return selected_name_; }
    QString SelectedVideoPath() const { return selected_path_; }

private slots:
    void OnSearchTextChanged(const QString& text);
    void OnDateFromChanged(const QDate& date);
    void OnDateToChanged(const QDate& date);
    void OnSelectionChanged();
    void OnSelectClicked();
    void OnDoubleClicked(const QModelIndex& index);

private:
    void SetupUI();
    void LoadVideos();

    std::shared_ptr<db::VideoRepo> repo_;
    int64_t selected_id_ = 0;
    QString selected_name_;
    QString selected_path_;

    QTableView* table_view_ = nullptr;
    QLineEdit* search_box_ = nullptr;
    DatePicker* date_from_ = nullptr;
    DatePicker* date_to_ = nullptr;
    QPushButton* select_btn_ = nullptr;
    VideoTableModel* model_ = nullptr;
    VideoFilterProxy* proxy_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
