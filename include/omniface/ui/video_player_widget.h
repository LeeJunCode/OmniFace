#pragma once

#include <QAudioOutput>
#include <QImage>
#include <QMediaPlayer>
#include <QPoint>
#include <QTimer>
#include <QVideoSink>
#include <QWidget>

#include <string>

namespace omniface::ui {

class VideoPlayerWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);
    ~VideoPlayerWidget() override;

    void setVideoPath(const std::string& path);
    void setVolume(int vol);
    int volume() const;
    bool isPlaying() const;

public slots:
    void play();
    void pause();
    void stop();

signals:
    void playingChanged(bool playing);
    void positionChanged(qint64 position);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onVideoFrameChanged(const QVideoFrame& frame);
    void onPositionChanged(qint64 pos);
    void onDurationChanged(qint64 dur);
    void onPlayerStateChanged(QMediaPlayer::PlaybackState state);
    void onHideControlsTimeout();

private:
    void paintPoster(QPainter& p);
    void paintVideoFrame(QPainter& p);
    void paintControls(QPainter& p);

    QString formatTime(qint64 ms) const;
    double progressAtPos(const QPoint& pos) const;
    QRectF centerPlayBtnRect() const;
    QRectF bottomBarRect() const;
    QRectF progressBarRect() const;
    QRectF volumeSliderRect() const;
    bool isOverProgressBar(const QPoint& pos) const;
    bool isOverVolumeSlider(const QPoint& pos) const;

    void showControls();
    void hideControls();

    QMediaPlayer* media_player_ = nullptr;
    QAudioOutput* audio_output_ = nullptr;
    QVideoSink* video_sink_ = nullptr;

    QImage poster_image_;
    QImage current_frame_;
    std::string video_path_;
    bool has_video_ = false;

    qint64 current_position_ = 0;
    qint64 total_duration_ = 0;

    QTimer* hide_controls_timer_ = nullptr;
    bool controls_visible_ = true;
    bool seeking_ = false;
    bool adjusting_volume_ = false;
    int volume_ = 100;
    bool muted_ = false;

    QPoint mouse_pos_;
    bool hovered_ = false;
};

}  // namespace omniface::ui
