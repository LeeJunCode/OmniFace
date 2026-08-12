#include "omniface/ui/widgets/video_player_widget.h"
#include "omniface/ui/theme.h"

#include <QApplication>
#include <QAudioOutput>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QSlider>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace omniface {
namespace ui {

namespace {

// ────────────────────────────────────────────────────────────
// SeekSlider — click-to-position + drag support
// ────────────────────────────────────────────────────────────

class SeekSlider : public QSlider {
public:
    using SeekCallback = std::function<void(int)>;

    explicit SeekSlider(SeekCallback onSeek, QWidget* parent = nullptr)
        : QSlider(Qt::Horizontal, parent), on_seek_(std::move(onSeek)) {}

protected:
    void mousePressEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton) {
            int val = QStyle::sliderValueFromPosition(
                minimum(), maximum(), static_cast<int>(ev->position().x()), width());
            setValue(val);
            on_seek_(val);
        }
        QSlider::mousePressEvent(ev);
    }

    void mouseMoveEvent(QMouseEvent* ev) override {
        if (ev->buttons() & Qt::LeftButton) {
            int val = QStyle::sliderValueFromPosition(
                minimum(), maximum(), static_cast<int>(ev->position().x()), width());
            setValue(val);
            on_seek_(val);
        }
        QSlider::mouseMoveEvent(ev);
    }

private:
    SeekCallback on_seek_;
};

}  // namespace

// ────────────────────────────────────────────────────────────
// VideoPlayerWidget
// ────────────────────────────────────────────────────────────

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);

    auto& tm = ThemeManager::Instance();

    // ── Graphics view + video item ─────────────────────────

    scene_ = new QGraphicsScene(this);
    scene_->setBackgroundBrush(QColor(tm.DeepBg()));

    video_item_ = new QGraphicsVideoItem();
    scene_->addItem(video_item_);

    view_ = new QGraphicsView(scene_, this);
    view_->setFrameShape(QFrame::NoFrame);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setRenderHint(QPainter::SmoothPixmapTransform);
    view_->setBackgroundBrush(QColor(tm.DeepBg()));
    view_->setStyleSheet(QStringLiteral("QGraphicsView { background: %1; border: none; }")
        .arg(tm.DeepBg()));

    connect(video_item_, &QGraphicsVideoItem::nativeSizeChanged, this, [this](const QSizeF& size) {
        video_item_->setSize(size);
        scene_->setSceneRect(QRectF(QPointF(0, 0), size));
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    });

    // ── Media backend ──────────────────────────────────────

    audio_output_ = new QAudioOutput(this);
    audio_output_->setVolume(1.0);

    player_ = new QMediaPlayer(this);
    player_->setAudioOutput(audio_output_);
    player_->setVideoOutput(video_item_);

    // ── Layout ─────────────────────────────────────────────

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(view_);

    // ── Control bar (child of this widget, overlaid on view) ──

    QColor bgBase(tm.DeepBg());
    QColor hoverBase(tm.HoverBg());

    control_bar_ = new QWidget(this);
    control_bar_->setObjectName("controlBar");
    control_bar_->setStyleSheet(QStringLiteral(
        "#controlBar {"
        "  background: rgba(%1,%2,%3,210);"
        "  border-radius: 8px;"
        "}"
        "#controlBar QPushButton {"
        "  background: transparent;"
        "  color: %4;"
        "  border: none;"
        "  border-radius: 3px;"
        "  font-size: 12px;"
        "}"
        "#controlBar QPushButton:hover {"
        "  background: rgba(%5,%6,%7,0.20);"
        "}"
        "#controlBar QLabel {"
        "  color: %8;"
        "  font-size: 11px;"
        "  background: transparent;"
        "}"
        "QSlider::groove:horizontal {"
        "  background: rgba(%5,%6,%7,0.20);"
        "  height: 4px;"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: %4;"
        "  width: 12px;"
        "  height: 12px;"
        "  margin: -4px 0;"
        "  border-radius: 6px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "  background: %9;"
        "  width: 14px;"
        "  height: 14px;"
        "  margin: -5px 0;"
        "  border-radius: 7px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: %10;"
        "  border-radius: 2px;"
        "}"
        "QSlider::groove:vertical {"
        "  background: rgba(%5,%6,%7,0.20);"
        "  width: 4px;"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:vertical {"
        "  background: %4;"
        "  width: 12px;"
        "  height: 12px;"
        "  margin: 0 -4px;"
        "  border-radius: 6px;"
        "}"
        "QSlider::sub-page:vertical {"
        "  background: %10;"
        "  border-radius: 2px;"
        "}")
        .arg(bgBase.red()).arg(bgBase.green()).arg(bgBase.blue())
        .arg(tm.TextPrimary())
        .arg(hoverBase.red()).arg(hoverBase.green()).arg(hoverBase.blue())
        .arg(tm.TextDim())
        .arg(tm.AccentBlue())
        .arg(tm.Primary()));

    auto* ctrl = new QHBoxLayout(control_bar_);
    ctrl->setContentsMargins(8, 4, 8, 4);
    ctrl->setSpacing(4);

    // Play / Pause
    play_pause_btn_ = new QPushButton(control_bar_);
    play_pause_btn_->setText(QStringLiteral("\u25B6"));
    play_pause_btn_->setFixedSize(28, 24);
    play_pause_btn_->setCursor(Qt::PointingHandCursor);
    play_pause_btn_->setToolTip(QStringLiteral("播放 / 暂停"));
    connect(play_pause_btn_, &QPushButton::clicked, this, &VideoPlayerWidget::OnPlayPause);
    ctrl->addWidget(play_pause_btn_);

    // Stop
    stop_btn_ = new QPushButton(control_bar_);
    stop_btn_->setText(QStringLiteral("\u25A0"));
    stop_btn_->setFixedSize(24, 24);
    stop_btn_->setCursor(Qt::PointingHandCursor);
    stop_btn_->setToolTip(QStringLiteral("停止"));
    connect(stop_btn_, &QPushButton::clicked, this, &VideoPlayerWidget::OnStop);
    ctrl->addWidget(stop_btn_);

    // Volume button
    volume_btn_ = new QPushButton(control_bar_);
    volume_btn_->setText(QStringLiteral("\u26A3"));
    volume_btn_->setFixedSize(24, 24);
    volume_btn_->setCursor(Qt::PointingHandCursor);
    volume_btn_->setToolTip(QStringLiteral("音量"));
    connect(volume_btn_, &QPushButton::clicked, this, &VideoPlayerWidget::OnMuteToggle);
    ctrl->addWidget(volume_btn_);

    // Volume slider (hidden by default)
    volume_slider_ = new QSlider(Qt::Vertical, control_bar_);
    volume_slider_->setRange(0, 100);
    volume_slider_->setValue(100);
    volume_slider_->setFixedHeight(60);
    volume_slider_->setCursor(Qt::PointingHandCursor);
    volume_slider_->hide();
    connect(volume_slider_, &QSlider::valueChanged, this, &VideoPlayerWidget::OnVolumeChanged);
    ctrl->addWidget(volume_slider_);

    // Time label
    time_label_ = new QLabel(QStringLiteral("00:00 / 00:00"), control_bar_);
    time_label_->setAlignment(Qt::AlignCenter);
    time_label_->setMinimumWidth(90);
    ctrl->addWidget(time_label_);

    // Seek slider
    seek_slider_ = new SeekSlider([this](int ms) {
        seeking_ = true;
        player_->setPosition(ms);
    }, control_bar_);
    seek_slider_->setRange(0, 0);
    seek_slider_->setCursor(Qt::PointingHandCursor);
    ctrl->addWidget(seek_slider_, 1);

    // Fullscreen
    fullscreen_btn_ = new QPushButton(control_bar_);
    fullscreen_btn_->setText(QStringLiteral("\u26F6"));
    fullscreen_btn_->setFixedSize(24, 24);
    fullscreen_btn_->setCursor(Qt::PointingHandCursor);
    fullscreen_btn_->setToolTip(QStringLiteral("全屏"));
    connect(fullscreen_btn_, &QPushButton::clicked, this, &VideoPlayerWidget::OnFullscreen);
    ctrl->addWidget(fullscreen_btn_);

    control_bar_->setFixedHeight(38);
    control_bar_->hide();
    control_bar_->installEventFilter(this);

    // ── Event filters for mouse tracking on video area ────

    view_->installEventFilter(this);
    view_->viewport()->installEventFilter(this);
    view_->viewport()->setMouseTracking(true);

    // ── Timers ─────────────────────────────────────────────

    hide_timer_.setSingleShot(true);
    hide_timer_.setInterval(2500);
    connect(&hide_timer_, &QTimer::timeout, this, &VideoPlayerWidget::OnHideTimer);

    // ── Player connections ─────────────────────────────────

    connect(player_, &QMediaPlayer::positionChanged,
            this, &VideoPlayerWidget::OnPositionChanged);
    connect(player_, &QMediaPlayer::durationChanged,
            this, &VideoPlayerWidget::OnDurationChanged);
    connect(player_, &QMediaPlayer::mediaStatusChanged,
            this, &VideoPlayerWidget::OnMediaStatusChanged);
}

VideoPlayerWidget::~VideoPlayerWidget() {
    player_->stop();
}

// ─── Public API ──────────────────────────────────────────────

void VideoPlayerWidget::LoadVideo(const QString& path) {
    pending_path_ = path;
    player_->setSource(QUrl::fromLocalFile(path));
}

void VideoPlayerWidget::SetPendingPath(const QString& path) {
    pending_path_ = path;
}

void VideoPlayerWidget::Stop() {
    player_->stop();
    UpdatePlayPauseIcon();
}

void VideoPlayerWidget::Reset() {
    player_->stop();
    player_->setSource(QUrl());
    pending_path_.clear();
    UpdatePlayPauseIcon();
}

qint64 VideoPlayerWidget::Position() const {
    return player_->position();
}

qint64 VideoPlayerWidget::Duration() const {
    return player_->duration();
}

void VideoPlayerWidget::SetControlsAlwaysVisible(bool visible) {
    controls_always_visible_ = visible;
    if (visible) {
        ShowControls();
    } else {
        hide_timer_.stop();
        control_bar_->hide();
    }
}

// ─── Slots ───────────────────────────────────────────────────

void VideoPlayerWidget::OnPlayPause() {
    if (player_->source().isEmpty() && !pending_path_.isEmpty()) {
        player_->setSource(QUrl::fromLocalFile(pending_path_));
    }
    if (player_->playbackState() == QMediaPlayer::PlayingState) {
        player_->pause();
    } else {
        player_->play();
    }
    UpdatePlayPauseIcon();
}

void VideoPlayerWidget::OnStop() {
    player_->stop();
    UpdatePlayPauseIcon();
}

void VideoPlayerWidget::OnMuteToggle() {
    if (audio_output_->volume() > 0.01) {
        saved_volume_ = static_cast<int>(audio_output_->volume() * 100);
        audio_output_->setMuted(true);
    } else {
        audio_output_->setMuted(false);
        audio_output_->setVolume(saved_volume_ / 100.0);
    }
    UpdateVolumeIcon();

    volume_slider_visible_ = !volume_slider_visible_;
    volume_slider_->setVisible(volume_slider_visible_);
}

void VideoPlayerWidget::OnVolumeChanged(int volume) {
    audio_output_->setMuted(false);
    audio_output_->setVolume(volume / 100.0);
    saved_volume_ = volume;
    UpdateVolumeIcon();
}

void VideoPlayerWidget::OnPositionChanged(qint64 ms) {
    if (!seeking_) {
        seek_slider_->blockSignals(true);
        seek_slider_->setValue(static_cast<int>(ms));
        seek_slider_->blockSignals(false);
    } else {
        seeking_ = false;
    }

    time_label_->setText(QStringLiteral("%1 / %2")
                             .arg(FormatTime(ms), FormatTime(player_->duration())));

    emit PositionChanged(ms);
}

void VideoPlayerWidget::OnDurationChanged(qint64 ms) {
    seek_slider_->setRange(0, static_cast<int>(ms));
    time_label_->setText(QStringLiteral("%1 / %2")
                             .arg(FormatTime(player_->position()), FormatTime(ms)));
    emit DurationChanged(ms);
}

void VideoPlayerWidget::OnMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        UpdatePlayPauseIcon();
    }
}

void VideoPlayerWidget::OnHideTimer() {
    if (!control_bar_)
        return;
    if (controls_always_visible_)
        return;
    QPoint gp = QCursor::pos();
    QPoint local = control_bar_->mapFromGlobal(gp);
    bool overControlBar = control_bar_->rect().contains(local);
    bool sliderActive = QApplication::mouseButtons() & Qt::LeftButton;

    if (overControlBar || sliderActive) {
        hide_timer_.start();
    } else {
        control_bar_->hide();
        volume_slider_->hide();
        volume_slider_visible_ = false;
    }
}

void VideoPlayerWidget::OnFullscreen() {
    if (fullscreen_) {
        // ── Exit fullscreen ──────────────────────────────
        view_->setWindowFlags(Qt::Widget);

        // Move control bar back to this
        control_bar_->setParent(this);
        RepositionControlBar();
        control_bar_->raise();
        if (!controls_always_visible_) {
            control_bar_->hide();
        }

        // Reinsert view into this layout
        if (auto* lay = qobject_cast<QVBoxLayout*>(layout())) {
            lay->insertWidget(0, view_);
        }
        view_->show();
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);

        fullscreen_ = false;
        fullscreen_btn_->setText(QStringLiteral("\u26F6"));
    } else {
        // ── Enter fullscreen ─────────────────────────────
        if (auto* lay = qobject_cast<QVBoxLayout*>(layout())) {
            lay->removeWidget(view_);
        }

        view_->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        view_->showFullScreen();

        // Move control bar to fullscreen view
        control_bar_->setParent(view_);
        QTimer::singleShot(100, this, [this]() {
            view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
            RepositionControlBar();
            control_bar_->raise();
            control_bar_->show();
        });

        view_->activateWindow();

        fullscreen_ = true;
        fullscreen_btn_->setText(QStringLiteral("\u2716"));
    }
}

// ─── Helpers ─────────────────────────────────────────────────

void VideoPlayerWidget::UpdatePlayPauseIcon() {
    bool playing = (player_->playbackState() == QMediaPlayer::PlayingState);
    play_pause_btn_->setText(playing ? QStringLiteral("\u23F8") : QStringLiteral("\u25B6"));
}

void VideoPlayerWidget::UpdateVolumeIcon() {
    bool muted = audio_output_->isMuted() || audio_output_->volume() < 0.01;
    if (muted)
        volume_btn_->setText(QStringLiteral("\u26A6"));
    else if (audio_output_->volume() < 0.5)
        volume_btn_->setText(QStringLiteral("\u26A4"));
    else
        volume_btn_->setText(QStringLiteral("\u26A3"));
}

void VideoPlayerWidget::RepositionControlBar() {
    if (!control_bar_)
        return;
    QWidget* p = control_bar_->parentWidget();
    int w = p ? p->width() : width();
    int h = p ? p->height() : height();
    int bw = std::min(w - 24, 720);
    int bh = control_bar_->height();
    control_bar_->setGeometry((w - bw) / 2, h - bh - 10, bw, bh);
}

void VideoPlayerWidget::ShowControls() {
    RepositionControlBar();
    control_bar_->raise();
    control_bar_->show();
    hide_timer_.start();
}

// ─── Events ──────────────────────────────────────────────────

void VideoPlayerWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (scene_->sceneRect().isValid() && !scene_->sceneRect().isEmpty())
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    RepositionControlBar();
}

void VideoPlayerWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (scene_->sceneRect().isValid() && !scene_->sceneRect().isEmpty())
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

void VideoPlayerWidget::mouseMoveEvent(QMouseEvent* event) {
    QWidget::mouseMoveEvent(event);
    ShowControls();
}

void VideoPlayerWidget::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    ShowControls();
}

void VideoPlayerWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    hide_timer_.start();
}

void VideoPlayerWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && fullscreen_) {
        OnFullscreen();
        return;
    }
    if (event->key() == Qt::Key_Space) {
        OnPlayPause();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool VideoPlayerWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == view_) {
        if (event->type() == QEvent::Resize) {
            // Defer so viewport has been resized before fitInView runs
            QTimer::singleShot(0, this, [this]() {
                if (scene_->sceneRect().isValid() && !scene_->sceneRect().isEmpty())
                    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
                if (fullscreen_)
                    RepositionControlBar();
            });
        }
    } else if (obj == view_->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::Enter:
            ShowControls();
            break;
        case QEvent::Leave:
            if (!controls_always_visible_) {
                hide_timer_.start();
            }
            break;
        case QEvent::KeyPress: {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape && fullscreen_) {
                OnFullscreen();
                return true;
            }
            if (ke->key() == Qt::Key_Space) {
                OnPlayPause();
                return true;
            }
            break;
        }
        default:
            break;
        }
    } else if (obj == control_bar_) {
        if (event->type() == QEvent::Enter) {
            ShowControls();
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ─── Formatting ──────────────────────────────────────────────

QString VideoPlayerWidget::FormatTime(qint64 ms) const {
    if (ms < 0)
        ms = 0;
    int sec = static_cast<int>(ms / 1000);
    int m = sec / 60;
    int s = sec % 60;
    return QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

}  // namespace ui
}  // namespace omniface
