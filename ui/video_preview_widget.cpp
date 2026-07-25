#include "omniface/ui/video_preview_widget.h"

#include <algorithm>
#include <cmath>

#include <opencv2/imgproc.hpp>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace omniface::ui {

VideoPreviewWidget::VideoPreviewWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(160, 90);
    setCursor(Qt::PointingHandCursor);

    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &VideoPreviewWidget::onTimerTick);
}

VideoPreviewWidget::~VideoPreviewWidget() {
    stop();
}

void VideoPreviewWidget::setVideoPath(const std::string& path) {
    stop();
    video_path_ = path;
    poster_frame_ = QImage();

    if (!cap_.open(path)) return;

    fps_ = cap_.get(cv::CAP_PROP_FPS);
    if (fps_ <= 0.0) fps_ = 24.0;
    total_frames_ = static_cast<int64_t>(cap_.get(cv::CAP_PROP_FRAME_COUNT));

    cv::Mat frame;
    if (cap_.read(frame) && !frame.empty()) {
        poster_frame_ = matToQImage(frame);
        current_frame_ = poster_frame_;
    }
    cap_.release();
    update();
}

void VideoPreviewWidget::setThumbnail(const QPixmap& pixmap) {
    poster_frame_ = pixmap.toImage();
    if (!playing_) {
        current_frame_ = poster_frame_;
        update();
    }
}

void VideoPreviewWidget::play() {
    if (video_path_.empty() || playing_) return;
    if (cap_.isOpened()) cap_.release();
    if (!cap_.open(video_path_)) return;

    playing_ = true;
    current_frame_idx_ = 0;
    updateFrame();
    timer_->start(static_cast<int>(1000.0 / fps_));
    emit playingChanged(true);
}

void VideoPreviewWidget::pause() {
    if (!playing_) return;
    timer_->stop();
    playing_ = false;
    emit playingChanged(false);
    update();
}

void VideoPreviewWidget::stop() {
    timer_->stop();
    playing_ = false;
    current_frame_idx_ = 0;
    if (cap_.isOpened()) cap_.release();
    current_frame_ = poster_frame_;
    emit playingChanged(false);
    update();
}

void VideoPreviewWidget::onTimerTick() {
    if (!playing_ || !cap_.isOpened()) return;

    cv::Mat frame;
    if (cap_.read(frame) && !frame.empty()) {
        current_frame_ = matToQImage(frame);
        current_frame_idx_ = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
        update();
    } else {
        pause();
        current_frame_idx_ = total_frames_;
        update();
    }
}

void VideoPreviewWidget::updateFrame() {
    cv::Mat frame;
    if (cap_.read(frame) && !frame.empty()) {
        current_frame_ = matToQImage(frame);
        current_frame_idx_ = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
    }
}

void VideoPreviewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QRectF r = rect();

    // Frame / poster
    if (!current_frame_.isNull()) {
        QImage scaled = current_frame_.scaled(
            static_cast<int>(r.width()), static_cast<int>(r.height()),
            Qt::KeepAspectRatio, Qt::SmoothTransformation);
        double x = (r.width() - scaled.width()) / 2.0;
        double y = (r.height() - scaled.height()) / 2.0;
        painter.drawImage(QPointF(x, y), scaled);

        // Dim overlay when hovered
        if (hovered_ && !playing_) {
            painter.fillRect(r, QColor(0, 0, 0, 80));
        }
    } else {
        painter.fillRect(r, QColor(24, 24, 37));
        painter.setPen(QColor(108, 112, 134));
        painter.drawText(r, Qt::AlignCenter,
                         QStringLiteral("\u672a\u9009\u62e9\u89c6\u9891\u6587\u4ef6"));
    }

    // Controls overlay
    if (hovered_ || playing_) {
        QRectF btn_rect = playButtonRect();

        if (playing_) {
            // Pause icon: two vertical bars
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 200));
            double bar_w = btn_rect.width() * 0.22;
            double bar_h = btn_rect.height() * 0.45;
            double cx = btn_rect.center().x();
            double cy = btn_rect.center().y();
            painter.drawRoundedRect(QRectF(cx - bar_w * 0.8, cy - bar_h / 2, bar_w, bar_h), 2, 2);
            painter.drawRoundedRect(QRectF(cx + bar_w * 0.1, cy - bar_h / 2, bar_w, bar_h), 2, 2);
        } else {
            // Semi-transparent circle bg
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 140));
            painter.drawEllipse(btn_rect);

            // Play triangle
            QPainterPath triangle;
            double cx = btn_rect.center().x();
            double cy = btn_rect.center().y();
            double s = btn_rect.width() * 0.22;
            triangle.moveTo(cx - s * 0.7, cy - s);
            triangle.lineTo(cx - s * 0.7, cy + s);
            triangle.lineTo(cx + s * 1.1, cy);
            triangle.closeSubpath();
            painter.setBrush(QColor(255, 255, 255, 220));
            painter.drawPath(triangle);
        }

        // Progress bar
        QRectF bar_rect = progressBarRect();
        double frac = (total_frames_ > 0)
                          ? static_cast<double>(current_frame_idx_) / total_frames_
                          : 0.0;

        // Track
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 60));
        painter.drawRoundedRect(bar_rect, 2, 2);

        // Progress
        if (frac > 0.0) {
            QRectF filled = bar_rect;
            filled.setWidth(bar_rect.width() * frac);
            painter.setBrush(QColor(137, 180, 250, 220));
            painter.drawRoundedRect(filled, 2, 2);
        }
    }
}

QRectF VideoPreviewWidget::playButtonRect() const {
    QRectF r = rect();
    double s = std::min(r.width(), r.height()) * 0.28;
    double cx = r.center().x();
    double cy = r.center().y();
    return QRectF(cx - s / 2, cy - s / 2, s, s);
}

QRectF VideoPreviewWidget::progressBarRect() const {
    QRectF r = rect();
    double margin = 8.0;
    double bar_h = 4.0;
    double y = r.bottom() - margin - bar_h;
    return QRectF(margin, y, r.width() - 2 * margin, bar_h);
}

void VideoPreviewWidget::enterEvent(QEnterEvent* /*event*/) {
    hovered_ = true;
    update();
}

void VideoPreviewWidget::leaveEvent(QEvent* /*event*/) {
    hovered_ = false;
    update();
}

void VideoPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    mouse_pos_ = event->pos();
    update();
}

void VideoPreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || video_path_.empty()) return;

    QRectF btn_rect = playButtonRect();
    QRectF bar_rect = progressBarRect();

    if (btn_rect.contains(event->position())) {
        if (playing_) {
            pause();
        } else {
            play();
        }
        return;
    }

    if (bar_rect.contains(event->position()) && total_frames_ > 0) {
        double frac = (event->position().x() - bar_rect.x()) / bar_rect.width();
        frac = std::clamp(frac, 0.0, 1.0);
        current_frame_idx_ = static_cast<int64_t>(frac * total_frames_);
        if (playing_) {
            cap_.set(cv::CAP_PROP_POS_FRAMES, current_frame_idx_);
            updateFrame();
        } else {
            bool need_release = !cap_.isOpened();
            if (need_release && !cap_.open(video_path_)) return;
            cap_.set(cv::CAP_PROP_POS_FRAMES, current_frame_idx_);
            cv::Mat frame;
            if (cap_.read(frame) && !frame.empty()) {
                current_frame_ = matToQImage(frame);
            }
            if (need_release) cap_.release();
        }
        update();
    }
}

void VideoPreviewWidget::resizeEvent(QResizeEvent* /*event*/) {
    update();
}

QImage VideoPreviewWidget::matToQImage(const cv::Mat& bgr) const {
    int w = bgr.cols, h = bgr.rows;
    if (qt_image_cache_.width() != w || qt_image_cache_.height() != h ||
        qt_image_cache_.format() != QImage::Format_RGB888) {
        qt_image_cache_ = QImage(w, h, QImage::Format_RGB888);
    }
    cv::Mat rgb_view(h, w, CV_8UC3, qt_image_cache_.bits(), qt_image_cache_.bytesPerLine());
    cv::cvtColor(bgr, rgb_view, cv::COLOR_BGR2RGB);
    return qt_image_cache_;
}

}  // namespace omniface::ui
