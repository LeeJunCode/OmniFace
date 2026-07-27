#pragma once

#include <string>

#include <opencv2/core/mat.hpp>

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

    MediaType type_;
    QScrollArea* scroll_ = nullptr;
    QWidget* content_ = nullptr;
    VideoPlayerWidget* video_preview_ = nullptr;
    QImage qt_image_cache_;
};

}  // namespace omniface::ui
