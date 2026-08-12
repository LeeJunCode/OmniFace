#pragma once

#include <QGraphicsVideoItem>
#include <QMediaPlayer>
#include <QTimer>
#include <QWidget>

#include <functional>

class QAudioOutput;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QPushButton;
class QSlider;

namespace omniface {
namespace ui {

class VideoPlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);
    ~VideoPlayerWidget() override;

    void LoadVideo(const QString& path);
    void SetPendingPath(const QString& path);
    void Stop();
    void Reset();
    qint64 Position() const;
    qint64 Duration() const;
    void SetControlsAlwaysVisible(bool visible);

signals:
    void PositionChanged(qint64 ms);
    void DurationChanged(qint64 ms);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void OnPlayPause();
    void OnStop();
    void OnMuteToggle();
    void OnFullscreen();
    void OnVolumeChanged(int volume);
    void OnPositionChanged(qint64 ms);
    void OnDurationChanged(qint64 ms);
    void OnMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void OnHideTimer();

private:
    void UpdatePlayPauseIcon();
    void UpdateVolumeIcon();
    void RepositionControlBar();
    void ShowControls();
    QString FormatTime(qint64 ms) const;

    // Graphics view + video item
    QGraphicsView* view_ = nullptr;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsVideoItem* video_item_ = nullptr;

    // Media
    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audio_output_ = nullptr;

    // Control bar
    QWidget* control_bar_ = nullptr;
    QPushButton* play_pause_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QPushButton* volume_btn_ = nullptr;
    QSlider* volume_slider_ = nullptr;
    bool volume_slider_visible_ = false;
    QLabel* time_label_ = nullptr;
    QSlider* seek_slider_ = nullptr;
    QPushButton* fullscreen_btn_ = nullptr;

    // State
    QTimer hide_timer_;
    bool seeking_ = false;
    bool fullscreen_ = false;
    bool controls_always_visible_ = false;
    int saved_volume_ = 100;
    QString pending_path_;
};

}  // namespace ui
}  // namespace omniface
