#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "omniface/ui/widgets/date_picker.h"

#include <memory>

namespace omniface {
namespace db {
class VideoRepo;
}

namespace ui {

class ImportVideoDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImportVideoDialog(db::VideoRepo* repo, QWidget* parent = nullptr);
    ~ImportVideoDialog() override;

    int64_t ImportedId() const { return imported_id_; }

private slots:
    void OnSelectFile();
    void OnImport();

private:
    db::VideoRepo* repo_;
    int64_t imported_id_ = 0;

    QLineEdit* path_edit_ = nullptr;
    QLabel* duration_label_ = nullptr;
    QLabel* resolution_label_ = nullptr;
    QLabel* frame_rate_label_ = nullptr;
    QLabel* file_size_label_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* description_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
    DatePicker* record_time_edit_ = nullptr;
    QLineEdit* location_edit_ = nullptr;
    QPushButton* import_btn_ = nullptr;

    QString source_path_;
    QString copied_path_;
    int64_t copied_id_ = 0;
    int64_t source_file_size_ = 0;

    // Cached metadata from OpenCV extraction
    double cached_fps_ = 24.0;
    int cached_width_ = 0;
    int cached_height_ = 0;
    double cached_duration_ = 0.0;
};

}  // namespace ui
}  // namespace omniface
