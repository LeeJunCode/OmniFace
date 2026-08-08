#pragma once

#include <QDialog>
#include <QLineEdit>

#include "omniface/ui/widgets/date_picker.h"

#include "omniface/db/video_repo.h"

namespace omniface {
namespace db {
class VideoRepo;
}

namespace ui {

class EditVideoDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditVideoDialog(db::VideoRepo* repo, int64_t video_id, QWidget* parent = nullptr);

private slots:
    void OnSave();

private:
    void SetupUI(const omniface::db::VideoRecord& rec);

    db::VideoRepo* repo_;
    int64_t video_id_;
    db::VideoRecord original_;

    QLineEdit* name_edit_ = nullptr;
    QLineEdit* description_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
    DatePicker* record_time_edit_ = nullptr;
    QLineEdit* location_edit_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
