#include "omniface/ui/video_player_widget.h"

#include <algorithm>
#include <cmath>

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoFrameFormat>

namespace omniface::ui {

// ============================================================
// Constructor / Destructor
// ============================================================

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent)
    : QWidget(parent) {

    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(160, 90);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Audio output
    audio_output_ = new QAudioOutput(this);
    audio_output_->setVolume(1.0);

    // Media player
    media_player_ = new QMediaPlayer(this);
    media_player_->setAudioOutput(audio_output_);

    // Video sink — receive decoded frames without native window
    video_sink_ = new QVideoSink(this);
    media_player_->setVideoSink(video_sink_);

    connect(video_sink_, &QVideoSink::videoFrameChanged,
            this, &VideoPlayerWidget::onVideoFrameChanged);
    connect(media_player_, &QMediaPlayer::positionChanged,
            this, &VideoPlayerWidget::onPositionChanged);
    connect(media_player_, &QMediaPlayer::durationChanged,
            this, &VideoPlayerWidget::onDurationChanged);
    connect(media_player_, &QMediaPlayer::playbackStateChanged,
            this, &VideoPlayerWidget::onPlayerStateChanged);

    // Auto-hide timer
    hide_controls_timer_ = new QTimer(this);
    hide_controls_timer_->setSingleShot(true);
    hide_controls_timer_->setInterval(2500);
    connect(hide_controls_timer_, &QTimer::timeout,
            this, &VideoPlayerWidget::onHideControlsTimeout);
}

VideoPlayerWidget::~VideoPlayerWidget() {
    stop();
}

// ============================================================
// Public API
// ============================================================

void VideoPlayerWidget::setVideoPath(const std::string& path) {
    stop();
    video_path_ = path;
    poster_image_ = QImage();
    current_frame_ = QImage();
    has_video_ = false;

    // Grab first frame as poster via OpenCV (no QMediaPlayer involvement)
    cv::VideoCapture cap(path);
    if (cap.isOpened()) {
        cv::Mat frame;
        if (cap.read(frame) && !frame.empty()) {
            cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
            poster_image_ = QImage(frame.data, frame.cols, frame.rows,
                                   static_cast<int>(frame.step),
                                   QImage::Format_RGB888).copy();
            has_video_ = true;
        }
        cap.release();
    }

    update();
}

void VideoPlayerWidget::setVolume(int vol) {
    volume_ = std::clamp(vol, 0, 100);
    muted_ = false;
    audio_output_->setMuted(false);
    audio_output_->setVolume(volume_ / 100.0f);
    update();
}

int VideoPlayerWidget::volume() const {
    return volume_;
}

bool VideoPlayerWidget::isPlaying() const {
    return media_player_->playbackState() == QMediaPlayer::PlayingState;
}

void VideoPlayerWidget::play() {
    if (video_path_.empty()) return;

    // Lazy-load source only when user explicitly plays
    if (media_player_->source().isEmpty()) {
        media_player_->setSource(QUrl::fromLocalFile(QString::fromStdString(video_path_)));
    }

    media_player_->play();
}

void VideoPlayerWidget::pause() {
    media_player_->pause();
}

void VideoPlayerWidget::stop() {
    hide_controls_timer_->stop();
    media_player_->stop();

    // Release file handle
    if (!media_player_->source().isEmpty()) {
        media_player_->setSource(QUrl());
    }

    current_frame_ = QImage();
    current_position_ = 0;
    total_duration_ = 0;
    controls_visible_ = true;
    update();
}

// ============================================================
// Video sink
// ============================================================

void VideoPlayerWidget::onVideoFrameChanged(const QVideoFrame& frame) {
    if (!frame.isValid()) return;
    QVideoFrame f(frame);
    if (f.map(QVideoFrame::ReadOnly)) {
        // Convert to QImage for painting — QVideoFrame formats vary by backend
        QImage img;
        if (f.pixelFormat() == QVideoFrameFormat::Format_ARGB8888 ||
            f.pixelFormat() == QVideoFrameFormat::Format_XRGB8888) {
            img = QImage(f.bits(0), f.width(), f.height(), f.bytesPerLine(0),
                         QImage::Format_ARGB32_Premultiplied).copy();
        } else if (f.pixelFormat() == QVideoFrameFormat::Format_BGRA8888 ||
                   f.pixelFormat() == QVideoFrameFormat::Format_BGRX8888) {
            img = QImage(f.bits(0), f.width(), f.height(), f.bytesPerLine(0),
                         QImage::Format_ARGB32_Premultiplied).rgbSwapped().copy();
        } else if (f.pixelFormat() == QVideoFrameFormat::Format_YUV420P ||
                   f.pixelFormat() == QVideoFrameFormat::Format_NV12) {
            // Common hardware formats — convert via QImage::convertTo
            img = f.toImage();
        } else {
            img = f.toImage();
        }
        f.unmap();

        if (!img.isNull()) {
            current_frame_ = img;
            update();
        }
    }
}

// ============================================================
// Media player slots
// ============================================================

void VideoPlayerWidget::onPositionChanged(qint64 pos) {
    if (!seeking_) {
        current_position_ = pos;
        update();
    }
}

void VideoPlayerWidget::onDurationChanged(qint64 dur) {
    total_duration_ = dur;
    update();
}

void VideoPlayerWidget::onPlayerStateChanged(QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::PlayingState) {
        hide_controls_timer_->start();
        emit playingChanged(true);
    } else if (state == QMediaPlayer::PausedState) {
        hide_controls_timer_->stop();
        controls_visible_ = true;
        update();
        emit playingChanged(false);
    } else {
        hide_controls_timer_->stop();
        controls_visible_ = true;
        update();
        emit playingChanged(false);
    }
}

void VideoPlayerWidget::onHideControlsTimeout() {
    if (media_player_->playbackState() == QMediaPlayer::PlayingState
        && !seeking_ && !adjusting_volume_) {
        controls_visible_ = false;
        setCursor(Qt::BlankCursor);
        update();
    }
}

// ============================================================
// Control visibility
// ============================================================

void VideoPlayerWidget::showControls() {
    controls_visible_ = true;
    setCursor(Qt::ArrowCursor);
    if (media_player_->playbackState() == QMediaPlayer::PlayingState) {
        hide_controls_timer_->start();
    }
    update();
}

void VideoPlayerWidget::hideControls() {
    // Only hide via timer timeout; not called directly
}

// ============================================================
// Paint — single paintEvent: poster / video frame / controls
// ============================================================

void VideoPlayerWidget::paintPoster(QPainter& p) {
    int w = width(), h = height();

    if (!poster_image_.isNull()) {
        QImage scaled = poster_image_.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int x = (w - scaled.width()) / 2;
        int y = (h - scaled.height()) / 2;
        p.drawImage(x, y, scaled);

        // Semi-transparent dim
        p.fillRect(rect(), QColor(0, 0, 0, 50));
    } else if (!has_video_) {
        p.fillRect(rect(), QColor(0xf0, 0xf0, 0xf0));
        p.setPen(QColor(0x99, 0x99, 0x99));
        QFont f = font();
        f.setPointSize(11);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("\u672a\u9009\u62e9\u89c6\u9891"));
        return;
    }

    // Center play button on poster
    QRectF btn = centerPlayBtnRect();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 140));
    p.drawEllipse(btn);

    QPainterPath triangle;
    double cx = btn.center().x(), cy = btn.center().y();
    double s = btn.width() * 0.24;
    triangle.moveTo(cx - s * 0.6, cy - s);
    triangle.lineTo(cx - s * 0.6, cy + s);
    triangle.lineTo(cx + s * 1.15, cy);
    triangle.closeSubpath();
    p.setBrush(QColor(255, 255, 255, 220));
    p.drawPath(triangle);
}

void VideoPlayerWidget::paintVideoFrame(QPainter& p) {
    if (current_frame_.isNull()) return;

    int w = width(), h = height();
    QImage scaled = current_frame_.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int x = (w - scaled.width()) / 2;
    int y = (h - scaled.height()) / 2;

    // Fill letterbox area
    p.fillRect(rect(), QColor(0, 0, 0));
    p.drawImage(x, y, scaled);
}

void VideoPlayerWidget::paintControls(QPainter& p) {
    bool playing = isPlaying();
    int w = width(), h = height();

    // Dim overlay
    p.fillRect(rect(), QColor(0, 0, 0, 50));

    // Center pause indicator (when paused)
    if (!playing) {
        QRectF btn = centerPlayBtnRect();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 140));
        p.drawEllipse(btn);

        QPainterPath triangle;
        double cx = btn.center().x(), cy = btn.center().y();
        double s = btn.width() * 0.24;
        triangle.moveTo(cx - s * 0.6, cy - s);
        triangle.lineTo(cx - s * 0.6, cy + s);
        triangle.lineTo(cx + s * 1.15, cy);
        triangle.closeSubpath();
        p.setBrush(QColor(255, 255, 255, 220));
        p.drawPath(triangle);
    }

    // --- Bottom control bar ---
    QRectF bar = bottomBarRect();
    double bar_h = bar.height();
    double bar_y = h - bar_h;

    p.fillRect(QRectF(0, bar_y, w, bar_h), QColor(0xf5, 0xf5, 0xf5, 230));

    // Scale factor relative to widget height for proportional sizing
    double scale = std::min(std::max(h / 360.0, 0.6), 1.5);

    // Play/pause button
    {
        double bs = 24.0 * scale;
        double bx = 10.0 * scale;
        double by = bar_y + (bar_h - bs) / 2;

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x55, 0x55, 0x55));
        if (playing) {
            double bw = std::max(bs * 0.26, 2.0);
            double bh = std::max(bs * 0.55, 4.0);
            double bcy = by + bs / 2;
            p.drawRoundedRect(QRectF(bx + bs * 0.25 - bw / 2, bcy - bh / 2, bw, bh), 2, 2);
            p.drawRoundedRect(QRectF(bx + bs * 0.75 - bw / 2, bcy - bh / 2, bw, bh), 2, 2);
        } else {
            QPainterPath tri;
            double bcy = by + bs / 2, bcx = bx + bs / 2 + 1;
            double ss = bs * 0.32;
            tri.moveTo(bcx - ss * 0.6, bcy - ss);
            tri.lineTo(bcx - ss * 0.6, bcy + ss);
            tri.lineTo(bcx + ss * 1.15, bcy);
            tri.closeSubpath();
            p.drawPath(tri);
        }
    }

    // Time font
    QFont time_font = font();
    time_font.setPointSize(std::max(static_cast<int>(9.0 * scale), 7));
    p.setFont(time_font);

    // Current time (after play button)
    {
        p.setPen(QColor(0x55, 0x55, 0x55));
        double tx = 38.0 * scale;  // play_btn(24) + gap(8) + offset(6)
        p.drawText(QRectF(tx, bar_y, 48 * scale, bar_h),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   formatTime(current_position_));
    }

    // Duration (between progress bar and volume icon)
    {
        QRectF pbar = progressBarRect();
        double dur_x = pbar.right() + 6.0 * scale;
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawText(QRectF(dur_x, bar_y, 50 * scale, bar_h),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   formatTime(total_duration_));
    }

    // Progress bar
    {
        QRectF pbar = progressBarRect();
        double frac = (total_duration_ > 0)
                          ? static_cast<double>(current_position_) / total_duration_
                          : 0.0;

        double pb_h = std::max(3.0 * scale, 2.0);
        double pb_y = bar_y + (bar_h - pb_h) / 2;
        QRectF track(pbar.x(), pb_y, pbar.width(), pb_h);
        p.fillRect(track, QColor(0xe0, 0xe0, 0xe0));

        if (frac > 0.0) {
            QRectF filled = track;
            filled.setWidth(track.width() * frac);
            p.fillRect(filled, QColor(137, 180, 250));
        }

        double hx = track.x() + track.width() * frac;
        double hr = std::max(4.0 * scale, 3.0);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x55, 0x55, 0x55));
        p.drawEllipse(QPointF(hx, track.center().y()), hr, hr);
    }

    // Volume
    {
        auto vs = volumeSliderRect();
        double pb_h = std::max(3.0 * scale, 2.0);
        double pb_y = bar_y + (bar_h - pb_h) / 2;
        QRectF track(vs.x(), pb_y, vs.width(), pb_h);

        // Volume icon (between duration text and volume slider)
        QFont vf = font();
        vf.setPointSize(std::max(static_cast<int>(9.0 * scale), 7));
        p.setFont(vf);
        p.setPen(QColor(0x55, 0x55, 0x55));
        double icon_w = 22.0 * scale;
        QRectF vrect(vs.x() - icon_w - 4.0 * scale, bar_y, icon_w, bar_h);
        p.drawText(vrect, Qt::AlignCenter,
                   (muted_ || volume_ == 0) ? QStringLiteral("\xF0\x9F\x94\x87")
                                            : QStringLiteral("\xF0\x9F\x94\x8A"));

        // Track
        p.fillRect(track, QColor(0xe0, 0xe0, 0xe0));
        double vfrac = muted_ ? 0.0 : volume_ / 100.0;
        if (vfrac > 0.0) {
            QRectF filled = track;
            filled.setWidth(track.width() * vfrac);
            p.fillRect(filled, QColor(137, 180, 250));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x55, 0x55, 0x55));
        double hr = std::max(3.0 * scale, 2.0);
        p.drawEllipse(QPointF(track.x() + track.width() * vfrac, track.center().y()), hr, hr);
    }
}

void VideoPlayerWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    bool playing = isPlaying();
    bool paused = media_player_->playbackState() == QMediaPlayer::PausedState;

    // Layer 1: Video frame or poster
    if ((playing || paused) && !current_frame_.isNull()) {
        paintVideoFrame(p);
    } else {
        paintPoster(p);
        // Don't paint controls in poster mode; center button is in paintPoster
        return;
    }

    // Layer 2: Controls (when visible)
    bool show_ctrl = controls_visible_ || !playing;
    if (show_ctrl) {
        paintControls(p);
    }
}

// ============================================================
// Layout helpers (all coordinates in widget-local space)
// ============================================================

QString VideoPlayerWidget::formatTime(qint64 ms) const {
    int total_sec = static_cast<int>(ms / 1000);
    return QString("%1:%2").arg(total_sec / 60, 2, 10, QChar('0'))
                           .arg(total_sec % 60, 2, 10, QChar('0'));
}

double VideoPlayerWidget::progressAtPos(const QPoint& pos) const {
    QRectF bar = progressBarRect();
    if (bar.width() <= 0) return 0.0;
    return std::clamp((pos.x() - bar.x()) / bar.width(), 0.0, 1.0);
}

QRectF VideoPlayerWidget::centerPlayBtnRect() const {
    double s = std::min({48.0, width() * 0.15, height() * 0.15});
    s = std::max(s, 32.0);
    return QRectF((width() - s) / 2, (height() - s) / 2, s, s);
}

QRectF VideoPlayerWidget::bottomBarRect() const {
    double bar_h = std::clamp(height() * 0.12, 32.0, 48.0);
    return QRectF(0, height() - bar_h, width(), bar_h);
}

QRectF VideoPlayerWidget::progressBarRect() const {
    double bar_h = bottomBarRect().height();
    double bar_y = height() - bar_h;
    double scale = std::min(std::max(height() / 360.0, 0.6), 1.5);

    // Left: play_btn(24) + gap(8) + cur_time(48) = 80 scaled
    double left = 80.0 * scale;
    // Right: duration(50) + gap(4) + vol_icon(22) + gap(4) + vol_slider(48) + pad(10) = 138 scaled
    double right = width() - 138.0 * scale;
    if (right - left < 40.0) {
        // Fallback for very narrow widgets
        left = 60.0;
        right = width() - 120.0;
    }
    return QRectF(left, bar_y, std::max(right - left, 40.0), bar_h);
}

QRectF VideoPlayerWidget::volumeSliderRect() const {
    double bar_h = bottomBarRect().height();
    double bar_y = height() - bar_h;
    double scale = std::min(std::max(height() / 360.0, 0.6), 1.5);
    double w = 48.0 * scale;
    return QRectF(width() - w - 10.0 * scale, bar_y, w, bar_h);
}

bool VideoPlayerWidget::isOverProgressBar(const QPoint& pos) const {
    QRectF bar = progressBarRect();
    return QRectF(bar.x(), bar.y() - 6, bar.width(), bar.height() + 12).contains(pos);
}

bool VideoPlayerWidget::isOverVolumeSlider(const QPoint& pos) const {
    QRectF bar = volumeSliderRect();
    return QRectF(bar.x(), bar.y() - 6, bar.width(), bar.height() + 12).contains(pos);
}

// ============================================================
// Events
// ============================================================

void VideoPlayerWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void VideoPlayerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    if (!has_video_) return;

    QPoint pos = event->pos();
    bool playing = isPlaying();
    bool paused_or_stopped = !playing;
    bool show_ctrl = controls_visible_ || paused_or_stopped;

    // Volume slider
    if (show_ctrl && isOverVolumeSlider(pos)) {
        adjusting_volume_ = true;
        QRectF vs = volumeSliderRect();
        double frac = (pos.x() - vs.x()) / vs.width();
        volume_ = std::clamp(static_cast<int>(frac * 100), 0, 100);
        muted_ = false;
        audio_output_->setMuted(false);
        audio_output_->setVolume(volume_ / 100.0f);
        update();
        return;
    }

    // Volume icon
    if (show_ctrl) {
        QRectF vs = volumeSliderRect();
        double scale = std::min(std::max(height() / 360.0, 0.6), 1.5);
        double icon_w = 22.0 * scale;
        QRectF vbtn(vs.x() - icon_w - 4.0 * scale, bottomBarRect().y(),
                    icon_w, bottomBarRect().height());
        if (vbtn.contains(pos)) {
            muted_ = !muted_;
            audio_output_->setMuted(muted_);
            update();
            return;
        }
    }

    // Progress bar
    if (show_ctrl && isOverProgressBar(pos)) {
        seeking_ = true;
        double frac = progressAtPos(pos);
        current_position_ = static_cast<qint64>(frac * total_duration_);
        media_player_->setPosition(current_position_);
        update();
        return;
    }

    // Small play/pause button in bottom bar
    if (show_ctrl) {
        QRectF bar = bottomBarRect();
        double bs = 24.0;
        QRectF sm_btn(10, bar.y() + (bar.height() - bs) / 2, bs, bs);
        if (sm_btn.contains(pos)) {
            goto toggle;
        }
    }

    // Center play button (poster or paused overlay)
    if (!playing) {
        if (centerPlayBtnRect().contains(pos)) {
            goto toggle;
        }
    }

    // Click anywhere else while playing: toggle controls visibility
    if (playing && !controls_visible_) {
        showControls();
        return;
    }

    // Click on video area toggles play/pause (web-like behavior)
toggle:
    if (playing) {
        pause();
    } else {
        play();
    }
}

void VideoPlayerWidget::mouseMoveEvent(QMouseEvent* event) {
    mouse_pos_ = event->pos();
    showControls();

    if (seeking_) {
        if (isOverProgressBar(event->pos())) {
            double frac = progressAtPos(event->pos());
            current_position_ = static_cast<qint64>(frac * total_duration_);
            media_player_->setPosition(current_position_);
        } else {
            seeking_ = false;
        }
        update();
        return;
    }

    if (adjusting_volume_) {
        if (isOverVolumeSlider(event->pos())) {
            QRectF vs = volumeSliderRect();
            double frac = (event->pos().x() - vs.x()) / vs.width();
            volume_ = std::clamp(static_cast<int>(frac * 100), 0, 100);
            muted_ = false;
            audio_output_->setMuted(false);
            audio_output_->setVolume(volume_ / 100.0f);
        } else {
            adjusting_volume_ = false;
        }
        update();
        return;
    }
}

void VideoPlayerWidget::enterEvent(QEnterEvent* /*event*/) {
    hovered_ = true;
    showControls();
}

void VideoPlayerWidget::leaveEvent(QEvent* /*event*/) {
    hovered_ = false;
    seeking_ = false;
    adjusting_volume_ = false;
}

void VideoPlayerWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Space:
        if (isPlaying()) pause(); else play();
        break;
    case Qt::Key_Left:
        media_player_->setPosition(std::max<qint64>(0, current_position_ - 5000));
        showControls();
        break;
    case Qt::Key_Right:
        media_player_->setPosition(std::min(total_duration_, current_position_ + 5000));
        showControls();
        break;
    case Qt::Key_M:
        muted_ = !muted_;
        audio_output_->setMuted(muted_);
        update();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

}  // namespace omniface::ui
