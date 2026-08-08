#pragma once

#include <QDialog>
#include <QLineEdit>

#include "omniface/db/task_repo.h"

namespace omniface {
namespace db {
class TaskRepo;
class VideoRepo;
class ImageRepo;
}

namespace ui {

class EditTaskDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditTaskDialog(db::TaskRepo* task_repo, db::VideoRepo* video_repo,
                            db::ImageRepo* image_repo, int64_t task_id,
                            QWidget* parent = nullptr);

private slots:
    void OnSave();

private:
    void SetupUI(const db::TaskRecord& task,
                 const QString& video_name, const QString& image_name);

    db::TaskRepo* task_repo_;
    db::VideoRepo* video_repo_;
    db::ImageRepo* image_repo_;
    int64_t task_id_;
    db::TaskRecord original_;

    QLineEdit* name_edit_ = nullptr;
    QLineEdit* description_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
