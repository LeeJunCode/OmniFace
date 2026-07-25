#pragma once

#include <memory>
#include <string>

#include <opencv2/videoio.hpp>

#include <QImage>
#include <QPoint>
#include <QTimer>
#include <QWidget>

namespace omniface::ui {

class VideoPreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoPreviewWidget(QWidget* parent = nullptr);
    ~VideoPreviewWidget() override;

    void setVideoPath(const std::string& path);
    void setThumbnail(const QPixmap& pixmap);
    bool isPlaying() const { return playing_; }

public slots:
    void play();
    void pause();
    void stop();

signals:
    void playingChanged(bool playing);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onTimerTick();

private:
    void updateFrame();
    QRectF playButtonRect() const;
    QRectF progressBarRect() const;
    QImage matToQImage(const cv::Mat& bgr) const;

    std::string video_path_;
    cv::VideoCapture cap_;
    QTimer* timer_ = nullptr;
    QImage current_frame_;
    QImage poster_frame_;
    mutable QImage qt_image_cache_;
    bool playing_ = false;
    bool hovered_ = false;
    double fps_ = 24.0;
    int64_t total_frames_ = 0;
    int64_t current_frame_idx_ = 0;
    QPoint mouse_pos_;
};

}  // namespace omniface::ui
