#pragma once

#include <QGroupBox>
#include <QLabel>
#include <QWidget>

#include <QPushButton>

#include <memory>

namespace omniface {
namespace db {
struct ImageRecord;
class ImageRepo;
}

namespace ui {

class ImageDetailPage : public QWidget {
    Q_OBJECT
public:
    explicit ImageDetailPage(std::shared_ptr<db::ImageRepo> repo, QWidget* parent = nullptr);

    void LoadImage(int64_t image_id);
    void ReapplyTheme();

signals:
    void BackRequested();
    void EditRequested(int64_t image_id);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void SetupUI();
    void Populate(const db::ImageRecord& rec);
    void RenderAnnotatedPixmap();

    std::shared_ptr<db::ImageRepo> repo_;
    int64_t image_id_ = 0;

    QImage original_image_;
    bool has_bbox_ = false;
    float bbox_x1_ = 0, bbox_y1_ = 0, bbox_x2_ = 0, bbox_y2_ = 0;
    float kp_[10]{};

    QLabel* name_label_ = nullptr;
    QLabel* description_label_ = nullptr;
    QLabel* notes_label_ = nullptr;
    QLabel* confidence_label_ = nullptr;
    QLabel* saved_path_label_ = nullptr;
    QLabel* created_at_label_ = nullptr;
    QLabel* image_display_ = nullptr;
    QWidget* embedding_panel_ = nullptr;

    QPushButton* back_btn_ = nullptr;
    QPushButton* edit_btn_ = nullptr;
    QPushButton* toggle_emb_btn_ = nullptr;
    QGroupBox* info_panel_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
