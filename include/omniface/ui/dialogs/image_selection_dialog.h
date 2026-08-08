#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>

#include <memory>

#include "omniface/types.h"

namespace omniface {
namespace db {
class ImageRepo;
struct ImageRecord;
}

namespace ui {

class ImageTableModel;
class ImageFilterProxy;

class ImageSelectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageSelectionDialog(std::shared_ptr<db::ImageRepo> repo, QWidget* parent = nullptr);

    int64_t SelectedImageId() const { return selected_id_; }
    QString SelectedImageName() const { return selected_name_; }
    FaceEmbedding SelectedEmbedding() const { return selected_embedding_; }

private slots:
    void OnSearchTextChanged(const QString& text);
    void OnSelectionChanged();
    void OnSelectClicked();
    void OnDoubleClicked(const QModelIndex& index);

private:
    void SetupUI();
    void LoadImages();

    std::shared_ptr<db::ImageRepo> repo_;
    int64_t selected_id_ = 0;
    QString selected_name_;
    FaceEmbedding selected_embedding_;

    QTableView* table_view_ = nullptr;
    QLineEdit* search_box_ = nullptr;
    QPushButton* select_btn_ = nullptr;
    ImageTableModel* model_ = nullptr;
    ImageFilterProxy* proxy_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
