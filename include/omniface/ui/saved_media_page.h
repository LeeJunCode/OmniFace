#pragma once

#include <set>
#include <string>

#include <opencv2/core/mat.hpp>

#include <QCheckBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "omniface/ui/video_player_widget.h"

namespace omniface::ui {

enum class MediaType { Video, Image };

class SavedMediaPage : public QWidget {
    Q_OBJECT

public:
    explicit SavedMediaPage(MediaType type, QWidget* parent = nullptr);
    void refresh();

signals:
    void backRequested();

private:
    void clearContent();
    void showVideos();
    void showImages();
    QImage matToQImage(const cv::Mat& bgr);

    void onSelectAll();
    void onBatchDelete();
    void updateBatchButtons();

    MediaType type_;
    QScrollArea* scroll_ = nullptr;
    QWidget* content_ = nullptr;
    VideoPlayerWidget* video_preview_ = nullptr;
    QImage qt_image_cache_;

    QCheckBox* select_all_cb_ = nullptr;
    QPushButton* batch_del_btn_ = nullptr;
    QWidget* batch_bar_ = nullptr;

    std::set<std::string> selected_;
    int total_items_ = 0;
};

}  // namespace omniface::ui
