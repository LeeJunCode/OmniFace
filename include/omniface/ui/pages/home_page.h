#pragma once

#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QWidget>

#include <memory>

namespace omniface {
namespace db {
class VideoRepo;
class ImageRepo;
class TaskRepo;
}

namespace ui {

class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(std::shared_ptr<db::VideoRepo> video_repo,
                      std::shared_ptr<db::ImageRepo> image_repo,
                      std::shared_ptr<db::TaskRepo> task_repo,
                      QWidget* parent = nullptr);

    void Refresh();
    void ReapplyTheme();

protected:
    void showEvent(QShowEvent* event) override { Refresh(); QWidget::showEvent(event); }

signals:
    void NavigateToProcessing();
    void NavigateToVideos();
    void NavigateToImages();
    void NavigateToHistory();

private:
    void SetupUI();
    std::shared_ptr<db::VideoRepo> video_repo_;
    std::shared_ptr<db::ImageRepo> image_repo_;
    std::shared_ptr<db::TaskRepo> task_repo_;

    QLabel* title_label_ = nullptr;
    QLabel* subtitle_label_ = nullptr;
    QLabel* actions_label_ = nullptr;
    QPushButton* proc_btn_ = nullptr;
    QPushButton* image_btn_ = nullptr;
    QPushButton* video_btn_ = nullptr;
    QPushButton* history_btn_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
