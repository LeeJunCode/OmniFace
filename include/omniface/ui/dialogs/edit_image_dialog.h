#pragma once

#include <QDialog>
#include <QLineEdit>

#include "omniface/db/image_repo.h"

#include <memory>

namespace omniface {
namespace ui {

class EditImageDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditImageDialog(db::ImageRepo* repo, int64_t image_id, QWidget* parent = nullptr);

private slots:
    void OnSave();

private:
    void SetupUI(const db::ImageRecord& rec);

    db::ImageRepo* repo_;
    int64_t image_id_;
    db::ImageRecord original_;

    QLineEdit* name_edit_ = nullptr;
    QLineEdit* description_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
