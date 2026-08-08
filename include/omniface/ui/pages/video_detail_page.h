#pragma once

#include <QLabel>
#include <QWidget>

#include <QPushButton>

#include <memory>

namespace omniface {
namespace db {
struct VideoRecord;
class VideoRepo;
}

namespace ui {

class VideoPlayerWidget;

class VideoDetailPage : public QWidget {
    Q_OBJECT
public:
    explicit VideoDetailPage(std::shared_ptr<db::VideoRepo> repo, QWidget* parent = nullptr);

    void LoadVideo(int64_t video_id);
    void ReapplyTheme();

signals:
    void BackRequested();
    void EditRequested(int64_t video_id);

private:
    void SetupUI();
    void Populate(const db::VideoRecord& rec);

    std::shared_ptr<db::VideoRepo> repo_;
    int64_t video_id_ = 0;

    VideoPlayerWidget* player_ = nullptr;

    QLabel* name_label_ = nullptr;
    QLabel* description_label_ = nullptr;
    QLabel* notes_label_ = nullptr;
    QLabel* duration_label_ = nullptr;
    QLabel* resolution_label_ = nullptr;
    QLabel* fps_label_ = nullptr;
    QLabel* record_time_label_ = nullptr;
    QLabel* location_label_ = nullptr;
    QLabel* file_size_label_ = nullptr;
    QLabel* saved_path_label_ = nullptr;
    QLabel* created_at_label_ = nullptr;

    QPushButton* back_btn_ = nullptr;
    QPushButton* edit_btn_ = nullptr;
    QWidget* info_panel_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
