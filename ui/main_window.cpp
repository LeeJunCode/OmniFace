#include "omniface/ui/main_window.h"

#include <chrono>
#include <cmath>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>

#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/align.h"
#include "omniface/core/recognition/face_recognizer.h"

namespace omniface::ui {

// ============================================================
// MainWindow
// ============================================================

MainWindow::MainWindow(const PipelineConfig& config, QWidget* parent)
    : QMainWindow(parent), config_(config) {
    setupUI();
    setupStyleSheet();
    resize(1280, 800);
    setWindowTitle(QStringLiteral("OmniFace - \u5b9e\u65f6\u4eba\u8138\u68c0\u6d4b\u4e0e\u8bc6\u522b"));
}

MainWindow::~MainWindow() {
    is_running_ = false;
    pipeline_.reset();
}

void MainWindow::setupUI() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* hlayout = new QHBoxLayout(central);
    hlayout->setContentsMargins(8, 8, 8, 8);
    hlayout->setSpacing(8);

    // ---- Left panel (scrollable) ----
    auto* scroll_area = new QScrollArea;
    scroll_area->setWidgetResizable(true);
    scroll_area->setMaximumWidth(340);
    scroll_area->setMinimumWidth(280);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* left_panel = new QWidget;
    auto* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(4, 4, 4, 4);
    left_layout->setSpacing(8);

    // ---- File selection ----
    auto* file_group = new QGroupBox(QStringLiteral("\u6587\u4ef6\u9009\u62e9"));
    auto* file_layout = new QVBoxLayout(file_group);

    open_video_btn_ = new QPushButton(QStringLiteral("\u9009\u62e9\u89c6\u9891..."));
    connect(open_video_btn_, &QPushButton::clicked, this, &MainWindow::onOpenVideo);
    file_layout->addWidget(open_video_btn_);

    video_preview_ = new VideoPreviewWidget;
    video_preview_->setObjectName("videoPreview");
    video_preview_->setFixedSize(280, 157);
    file_layout->addWidget(video_preview_, 0, Qt::AlignCenter);

    open_ref_btn_ = new QPushButton(QStringLiteral("\u9009\u62e9\u76ee\u6807\u4eba\u7269\u56fe\u50cf..."));
    connect(open_ref_btn_, &QPushButton::clicked, this, &MainWindow::onOpenReference);
    file_layout->addWidget(open_ref_btn_);

    // Dual reference image display: original + detected
    auto* ref_dual_layout = new QHBoxLayout;

    ref_original_thumbnail_ = new QLabel;
    ref_original_thumbnail_->setObjectName("refThumbnail");
    ref_original_thumbnail_->setFixedSize(130, 130);
    ref_original_thumbnail_->setAlignment(Qt::AlignCenter);
    ref_original_thumbnail_->setText(QStringLiteral("\u539f\u59cb\u56fe\u50cf"));
    ref_dual_layout->addWidget(ref_original_thumbnail_);

    ref_detected_thumbnail_ = new QLabel;
    ref_detected_thumbnail_->setObjectName("refThumbnail");
    ref_detected_thumbnail_->setFixedSize(130, 130);
    ref_detected_thumbnail_->setAlignment(Qt::AlignCenter);
    ref_detected_thumbnail_->setText(QStringLiteral("\u68c0\u6d4b\u4eba\u8138"));
    ref_dual_layout->addWidget(ref_detected_thumbnail_);

    file_layout->addLayout(ref_dual_layout);

    left_layout->addWidget(file_group);

    // ---- Playback controls ----
    auto* ctrl_group = new QGroupBox(QStringLiteral("\u64ad\u653e\u63a7\u5236"));
    auto* ctrl_layout = new QVBoxLayout(ctrl_group);

    auto* btn_row = new QHBoxLayout;
    play_btn_ = new QPushButton(QStringLiteral("\u25b6 \u64ad\u653e"));
    play_btn_->setObjectName("playBtn");
    play_btn_->setEnabled(false);
    connect(play_btn_, &QPushButton::clicked, this, &MainWindow::onPlay);
    btn_row->addWidget(play_btn_);

    pause_btn_ = new QPushButton(QStringLiteral("\u23f8 \u6682\u505c"));
    pause_btn_->setEnabled(false);
    connect(pause_btn_, &QPushButton::clicked, this, &MainWindow::onPause);
    btn_row->addWidget(pause_btn_);

    stop_btn_ = new QPushButton(QStringLiteral("\u25a0 \u505c\u6b62"));
    stop_btn_->setObjectName("stopBtn");
    stop_btn_->setEnabled(false);
    connect(stop_btn_, &QPushButton::clicked, this, &MainWindow::onStop);
    btn_row->addWidget(stop_btn_);

    ctrl_layout->addLayout(btn_row);

    auto* btn_row2 = new QHBoxLayout;
    step_btn_ = new QPushButton(QStringLiteral("\u23ed \u9010\u5e27"));
    step_btn_->setEnabled(false);
    connect(step_btn_, &QPushButton::clicked, this, &MainWindow::onStep);
    btn_row2->addWidget(step_btn_);

    replay_btn_ = new QPushButton(QStringLiteral("\u21ba \u91cd\u65b0\u64ad\u653e"));
    replay_btn_->setObjectName("replayBtn");
    replay_btn_->setEnabled(false);
    connect(replay_btn_, &QPushButton::clicked, this, &MainWindow::onReplay);
    btn_row2->addWidget(replay_btn_);

    ctrl_layout->addLayout(btn_row2);

    left_layout->addWidget(ctrl_group);

    // ---- Progress ----
    auto* prog_group = new QGroupBox(QStringLiteral("\u64ad\u653e\u8fdb\u5ea6"));
    auto* prog_layout = new QVBoxLayout(prog_group);

    progress_slider_ = new QSlider(Qt::Horizontal);
    progress_slider_->setObjectName("progressSlider");
    progress_slider_->setRange(0, 1000);
    progress_slider_->setEnabled(false);
    prog_layout->addWidget(progress_slider_);

    time_display_ = new QLabel("00:00 / 00:00");
    time_display_->setAlignment(Qt::AlignCenter);
    prog_layout->addWidget(time_display_);

    left_layout->addWidget(prog_group);

    // ---- Thresholds ----
    auto* thresh_group = new QGroupBox(QStringLiteral("\u68c0\u6d4b\u53c2\u6570"));
    auto* thresh_layout = new QFormLayout(thresh_group);

    match_threshold_slider_ = new QSlider(Qt::Horizontal);
    match_threshold_slider_->setRange(10, 90);
    match_threshold_slider_->setValue(static_cast<int>(config_.thresholds.match_threshold * 100));
    connect(match_threshold_slider_, &QSlider::valueChanged,
            this, &MainWindow::onMatchThresholdChanged);

    match_value_label_ = new QLabel(QString::number(config_.thresholds.match_threshold, 'f', 2));
    auto* match_row = new QHBoxLayout;
    match_row->addWidget(match_threshold_slider_);
    match_row->addWidget(match_value_label_);
    thresh_layout->addRow(QStringLiteral("\u5339  \u914d  \u9608  \u503c:"), match_row);

    detect_confidence_slider_ = new QSlider(Qt::Horizontal);
    detect_confidence_slider_->setRange(10, 90);
    detect_confidence_slider_->setValue(
        static_cast<int>(config_.thresholds.detection_confidence * 100));
    connect(detect_confidence_slider_, &QSlider::valueChanged,
            this, &MainWindow::onDetectConfidenceChanged);

    detect_value_label_ = new QLabel(
        QString::number(config_.thresholds.detection_confidence, 'f', 2));
    auto* detect_row = new QHBoxLayout;
    detect_row->addWidget(detect_confidence_slider_);
    detect_row->addWidget(detect_value_label_);
    thresh_layout->addRow(QStringLiteral("\u68c0\u6d4b\u7f6e\u4fe1\u5ea6:"), detect_row);

    left_layout->addWidget(thresh_group);

    // ---- Statistics ----
    auto* stats_group = new QGroupBox(QStringLiteral("\u5b9e\u65f6\u7edf\u8ba1"));
    auto* stats_layout = new QFormLayout(stats_group);

    fps_display_ = new QLabel("--");
    stats_layout->addRow(QStringLiteral("\u5904\u7406\u5e27\u7387:"), fps_display_);

    face_count_display_ = new QLabel("--");
    stats_layout->addRow(QStringLiteral("\u68c0\u6d4b\u4eba\u8138:"), face_count_display_);

    target_status_display_ = new QLabel(QStringLiteral("\u672a\u5f00\u59cb"));
    stats_layout->addRow(QStringLiteral("\u76ee\u6807\u72b6\u6001:"), target_status_display_);

    left_layout->addWidget(stats_group);
    left_layout->addStretch();

    scroll_area->setWidget(left_panel);

    // ---- Center: video display ----
    video_label_ = new QLabel(QStringLiteral("\u8bf7\u9009\u62e9\u89c6\u9891\u6587\u4ef6\u548c\u76ee\u6807\u4eba\u7269\u56fe\u50cf\uff0c\u7136\u540e\u70b9\u51fb\u64ad\u653e"));
    video_label_->setObjectName("videoLabel");
    video_label_->setAlignment(Qt::AlignCenter);
    video_label_->setMinimumSize(640, 480);

    // ---- Layout assembly ----
    hlayout->addWidget(scroll_area);
    hlayout->addWidget(video_label_, 1);

    // ---- Status bar ----
    status_label_ = new QLabel(QStringLiteral("\u5c31\u7eea"));
    statusBar()->addWidget(status_label_, 1);

    // ---- Menu bar ----
    auto* file_menu = menuBar()->addMenu(QStringLiteral("\u6587\u4ef6(&F)"));
    file_menu->addAction(QStringLiteral("\u6253\u5f00\u89c6\u9891(&V)..."), this, &MainWindow::onOpenVideo);
    file_menu->addAction(QStringLiteral("\u6253\u5f00\u76ee\u6807\u4eba\u7269\u56fe\u50cf(&R)..."), this, &MainWindow::onOpenReference);
    file_menu->addSeparator();
    file_menu->addAction(QStringLiteral("\u9000\u51fa(&X)"), this, &QWidget::close);

    auto* help_menu = menuBar()->addMenu(QStringLiteral("\u5e2e\u52a9(&H)"));
    help_menu->addAction(QStringLiteral("\u5173\u4e8e(&A)"), this, [this]() {
        QMessageBox::about(this, QStringLiteral("\u5173\u4e8e OmniFace"),
                           QStringLiteral("OmniFace - \u5b9e\u65f6\u4eba\u8138\u68c0\u6d4b\u3001\u8ddf\u8e2a\u4e0e\u8bc6\u522b\n\n"
                                          "\u57fa\u4e8e SCRFD + ByteTrack + MobileFaceNet"));
    });
}

void MainWindow::setupStyleSheet() {
    setStyleSheet(QStringLiteral(
        "QMainWindow { background-color: #1e1e2e; }"
        "QWidget { color: #cdd6f4; font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif; font-size: 13px; }"
        "QGroupBox { border: 1px solid #45475a; border-radius: 6px; margin-top: 12px; padding-top: 18px; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #89b4fa; }"
        "QPushButton { background-color: #313244; border: 1px solid #45475a; border-radius: 4px; padding: 6px 14px; color: #cdd6f4; min-height: 28px; }"
        "QPushButton:hover { background-color: #45475a; border-color: #585b70; }"
        "QPushButton:pressed { background-color: #585b70; }"
        "QPushButton:disabled { background-color: #1e1e2e; color: #6c7086; border-color: #313244; }"
        "QPushButton#playBtn { background-color: #1e5631; border-color: #4caf50; color: #ffffff; }"
        "QPushButton#playBtn:hover { background-color: #2e7d32; }"
        "QPushButton#playBtn:disabled { background-color: #1e1e2e; color: #6c7086; border-color: #313244; }"
        "QPushButton#stopBtn { background-color: #4a1a1a; border-color: #e53935; }"
        "QPushButton#stopBtn:hover { background-color: #c62828; }"
        "QPushButton#stopBtn:disabled { background-color: #1e1e2e; color: #6c7086; border-color: #313244; }"
        "QPushButton#replayBtn { background-color: #1e3a5f; border-color: #42a5f5; }"
        "QPushButton#replayBtn:hover { background-color: #1565c0; }"
        "QPushButton#replayBtn:disabled { background-color: #1e1e2e; color: #6c7086; border-color: #313244; }"
        "QSlider::groove:horizontal { background: #313244; height: 6px; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #89b4fa; width: 16px; margin: -5px 0; border-radius: 8px; }"
        "QSlider::sub-page:horizontal { background: #89b4fa; border-radius: 3px; }"
        "QSlider::handle:horizontal:disabled { background: #585b70; }"
        "QLabel { background: transparent; border: none; }"
        "QLabel#videoLabel { background-color: #11111b; border: 2px solid #313244; border-radius: 8px; color: #585b70; font-size: 15px; }"
        "QLabel#refThumbnail { background-color: #181825; border: 1px solid #45475a; border-radius: 4px; color: #6c7086; font-size: 12px; }"
        "QStatusBar { background-color: #181825; border-top: 1px solid #313244; color: #a6adc8; }"
        "QScrollArea { border: none; background: transparent; }"
        "QMenuBar { background-color: #1e1e2e; border-bottom: 1px solid #313244; padding: 2px; }"
        "QMenuBar::item { padding: 4px 12px; border-radius: 4px; }"
        "QMenuBar::item:selected { background-color: #45475a; }"
        "QMenu { background-color: #1e1e2e; border: 1px solid #45475a; padding: 4px; }"
        "QMenu::item { padding: 6px 28px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #45475a; }"
        "QMenu::separator { height: 1px; background: #45475a; margin: 4px 8px; }"
        "QSlider#progressSlider::groove:horizontal { height: 4px; }"
        "QSlider#progressSlider::handle:horizontal { width: 12px; margin: -4px 0; }"
    ));
}

QImage MainWindow::matToQImage(const cv::Mat& bgr) {
    int w = bgr.cols, h = bgr.rows;
    if (qt_image_cache_.width() != w || qt_image_cache_.height() != h ||
        qt_image_cache_.format() != QImage::Format_RGB888) {
        qt_image_cache_ = QImage(w, h, QImage::Format_RGB888);
    }
    cv::Mat rgb_view(h, w, CV_8UC3, qt_image_cache_.bits(), qt_image_cache_.bytesPerLine());
    cv::cvtColor(bgr, rgb_view, cv::COLOR_BGR2RGB);
    return qt_image_cache_;
}

void MainWindow::updateRefThumbnails() {
    if (reference_path_.empty()) return;

    cv::Mat ref_img = cv::imread(reference_path_);
    if (ref_img.empty()) return;

    // Original image thumbnail
    {
        QImage qimg = matToQImage(ref_img);
        QPixmap pixmap = QPixmap::fromImage(qimg);
        if (!pixmap.isNull()) {
            pixmap = pixmap.scaled(ref_original_thumbnail_->size(), Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
            ref_original_thumbnail_->setPixmap(pixmap);
        }
    }

    // Detected face image thumbnail
    {
        cv::Mat detected = ref_img.clone();
        try {
            detection::FaceDetector::Config det_cfg;
            det_cfg.intra_threads = config_.model.detection_threads;
            det_cfg.input_width = config_.model.detection_input_width;
            det_cfg.input_height = config_.model.detection_input_height;
            det_cfg.confidence_threshold = 0.5f;  // lower threshold for reference image
            det_cfg.nms_threshold = config_.thresholds.detection_nms;
            detection::FaceDetector detector(config_.model.detector_model, det_cfg);

            auto boxes = detector.Detect(ref_img);
            for (const auto& b : boxes) {
                cv::Rect r(static_cast<int>(b.x1), static_cast<int>(b.y1),
                           static_cast<int>(b.x2 - b.x1), static_cast<int>(b.y2 - b.y1));
                cv::rectangle(detected, r, cv::Scalar(0, 220, 0), 4);
                char conf_text[32];
                std::snprintf(conf_text, sizeof(conf_text), "%.2f", b.confidence);
                cv::putText(detected, conf_text,
                           cv::Point(r.x, r.y - 8),
                           cv::FONT_HERSHEY_SIMPLEX, 0.7,
                           cv::Scalar(0, 220, 0), 3, cv::LINE_AA);
                for (int k = 0; k < 5; ++k) {
                    cv::Point kp(static_cast<int>(b.keypoints[k * 2]),
                                 static_cast<int>(b.keypoints[k * 2 + 1]));
                    cv::circle(detected, kp, 3, cv::Scalar(0, 0, 255), -1);
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Face detection failed on reference image: {}", e.what());
        } catch (...) {
            spdlog::error("Face detection failed on reference image: unknown error");
        }

        QImage qimg = matToQImage(detected);
        QPixmap pixmap = QPixmap::fromImage(qimg);
        if (!pixmap.isNull()) {
            pixmap = pixmap.scaled(ref_detected_thumbnail_->size(), Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
            ref_detected_thumbnail_->setPixmap(pixmap);
        }
    }
}

// ============================================================
// Slots
// ============================================================

void MainWindow::onOpenVideo() {
    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("\u9009\u62e9\u89c6\u9891\u6587\u4ef6"),
        "",
        QStringLiteral("\u89c6\u9891\u6587\u4ef6 (*.mp4 *.avi *.mkv *.mov);;\u6240\u6709\u6587\u4ef6 (*)"));
    if (path.isEmpty()) return;

    video_path_ = path.toStdString();
    video_preview_->setVideoPath(video_path_);
    updateStateDisplay();
}

void MainWindow::onOpenReference() {
    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("\u9009\u62e9\u76ee\u6807\u4eba\u7269\u56fe\u50cf"),
        "",
        QStringLiteral("\u56fe\u50cf\u6587\u4ef6 (*.jpg *.jpeg *.png *.bmp);;\u6240\u6709\u6587\u4ef6 (*)"));
    if (path.isEmpty()) return;

    reference_path_ = path.toStdString();
    config_.model.reference_image = reference_path_;
    updateRefThumbnails();

    // Pre-extract and cache reference face embedding
    reference_cached_ = false;
    cached_reference_embedding_ = FaceEmbedding{};
    try {
        cv::Mat ref_img = cv::imread(reference_path_);
        if (!ref_img.empty()) {
            detection::FaceDetector::Config det_cfg;
            det_cfg.intra_threads = config_.model.detection_threads;
            det_cfg.input_width = config_.model.detection_input_width;
            det_cfg.input_height = config_.model.detection_input_height;
            det_cfg.confidence_threshold = config_.thresholds.detection_confidence;
            det_cfg.nms_threshold = config_.thresholds.detection_nms;
            detection::FaceDetector detector(config_.model.detector_model, det_cfg);

            auto boxes = detector.Detect(ref_img);
            if (!boxes.empty()) {
                const BoundingBox* best = &boxes[0];
                for (const auto& b : boxes) {
                    if (b.confidence > best->confidence) best = &b;
                }
                cv::Mat face_crop = recognition::AlignFace(ref_img, *best);
                if (!face_crop.empty()) {
                    recognition::FaceRecognizer::Config rec_cfg;
                    rec_cfg.intra_threads = config_.model.recognition_threads;
                    recognition::FaceRecognizer recognizer(config_.model.recognizer_model, rec_cfg);
                    cached_reference_embedding_ = recognizer.Extract(face_crop);
                    reference_cached_ = cached_reference_embedding_.valid;
                    if (reference_cached_) {
                        spdlog::info("Reference face embedding cached (confidence={:.2f})",
                                     best->confidence);
                    }
                }
            } else {
                spdlog::warn("No face detected in reference image");
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to cache reference embedding: {}", e.what());
    }

    updateStateDisplay();
}

void MainWindow::onPlay() {
    if (video_path_.empty()) {
        QMessageBox::warning(this,
                             QStringLiteral("\u9519\u8bef"),
                             QStringLiteral("\u8bf7\u5148\u9009\u62e9\u4e00\u4e2a\u89c6\u9891\u6587\u4ef6\u3002"));
        return;
    }
    if (reference_path_.empty()) {
        QMessageBox::warning(this,
                             QStringLiteral("\u9519\u8bef"),
                             QStringLiteral("\u8bf7\u5148\u9009\u62e9\u76ee\u6807\u4eba\u7269\u56fe\u50cf\u3002"));
        return;
    }

    video_preview_->pause();

    // Apply current threshold values
    config_.thresholds.match_threshold = match_threshold_slider_->value() / 100.0f;
    config_.thresholds.detection_confidence = detect_confidence_slider_->value() / 100.0f;
    config_.model.reference_image = reference_path_;
    config_.video.input_path = video_path_;

    try {
        pipeline_ = std::make_unique<Pipeline>(config_);
        if (reference_cached_) {
            pipeline_->EnrollReference(cached_reference_embedding_);
        }
        pipeline_->OpenVideo(video_path_);
    } catch (const std::exception& e) {
        QMessageBox::critical(this,
                              QStringLiteral("\u9519\u8bef"),
                              QStringLiteral("\u521d\u59cb\u5316\u7ba1\u7ebf\u5931\u8d25: %1").arg(e.what()));
        return;
    }

    video_fps_ = pipeline_->GetFps();
    total_frame_count_ = pipeline_->GetFrameCount();

    if (total_frame_count_ > 0) {
        progress_slider_->setRange(0, total_frame_count_);
        int total_sec = static_cast<int>(total_frame_count_ / video_fps_);
        time_display_->setText(QString("00:00 / %1:%2")
            .arg(total_sec / 60, 2, 10, QChar('0'))
            .arg(total_sec % 60, 2, 10, QChar('0')));
    }
    progress_slider_->setEnabled(true);

    is_running_ = true;
    is_paused_ = false;
    step_requested_ = false;
    has_finished_once_ = false;
    frame_count_ = 0;
    fps_frame_count_ = 0;
    current_fps_ = 0.0;
    last_fps_time_ = std::chrono::steady_clock::now();

    setUiEnabled(true);
    status_label_->setText(QStringLiteral("\u6b63\u5728\u5904\u7406..."));

    // Kick off the processing loop
    QTimer::singleShot(0, this, &MainWindow::processNextFrame);
}

void MainWindow::onPause() {
    if (!is_running_) return;

    if (is_paused_) {
        is_paused_ = false;
        step_requested_ = false;
        pause_btn_->setText(QStringLiteral("\u23f8 \u6682\u505c"));
        step_btn_->setEnabled(false);
        status_label_->setText(QStringLiteral("\u6b63\u5728\u5904\u7406..."));
        QTimer::singleShot(0, this, &MainWindow::processNextFrame);
    } else {
        is_paused_ = true;
        pause_btn_->setText(QStringLiteral("\u25b6 \u7ee7\u7eed"));
        step_btn_->setEnabled(true);
        status_label_->setText(QStringLiteral("\u5df2\u6682\u505c"));
    }
}

void MainWindow::onStop() {
    is_running_ = false;
    is_paused_ = false;
    step_requested_ = false;

    pipeline_.reset();

    pause_btn_->setText(QStringLiteral("\u23f8 \u6682\u505c"));
    step_btn_->setEnabled(false);
    status_label_->setText(QStringLiteral("\u5df2\u505c\u6b62"));

    if (total_frame_count_ > 0) {
        progress_slider_->setRange(0, total_frame_count_);
        progress_slider_->setValue(0);
    } else {
        progress_slider_->setRange(0, 1000);
        progress_slider_->setValue(0);
    }
    progress_slider_->setEnabled(false);
    time_display_->setText("00:00 / 00:00");

    setUiEnabled(false);
}

void MainWindow::onStep() {
    if (!is_running_ || !is_paused_) return;
    step_requested_ = true;
    status_label_->setText(QStringLiteral("\u5df2\u6682\u505c  |  \u9010\u5e27\u6a21\u5f0f"));
    QTimer::singleShot(0, this, &MainWindow::processNextFrame);
}

void MainWindow::onReplay() {
    onStop();
    QTimer::singleShot(200, this, &MainWindow::onPlay);
}

void MainWindow::processNextFrame() {
    if (!is_running_) return;

    if (is_paused_ && !step_requested_) return;

    cv::Mat frame;
    if (!pipeline_->PollFrame(frame)) {
        if (pipeline_->IsDecodeFinished()) {
            // True end of video
            is_running_ = false;
            is_paused_ = false;
            has_finished_once_ = true;
            pipeline_.reset();
            setFinishedState();
            status_label_->setText(QStringLiteral("\u5904\u7406\u5b8c\u6210"));
            if (total_frame_count_ > 0) {
                progress_slider_->setValue(total_frame_count_);
            }
            return;
        }
        // Queue temporarily empty, retry shortly
        QTimer::singleShot(2, this, &MainWindow::processNextFrame);
        return;
    }

    frame_count_++;
    fps_frame_count_++;

    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_fps_time_).count();
    if (elapsed >= 1.0) {
        current_fps_ = fps_frame_count_ / elapsed;
        fps_frame_count_ = 0;
        last_fps_time_ = now;
    }

    auto result = pipeline_->ProcessFrame(frame, frame_count_);

    // Display frame
    QImage qimg = matToQImage(result.annotated_frame);
    QPixmap pixmap = QPixmap::fromImage(qimg);
    if (!pixmap.isNull()) {
        pixmap = pixmap.scaled(video_label_->size(), Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
        video_label_->setPixmap(pixmap);
    }

    // Check if target found
    bool target_found = false;
    for (const auto& mr : result.match_results) {
        if (mr.is_match) { target_found = true; break; }
    }

    updateStatsDisplay(current_fps_, result.face_count, target_found);

    // Update progress
    if (total_frame_count_ > 0 && !progress_slider_->isSliderDown()) {
        progress_slider_->setValue(frame_count_);
    }
    int elapsed_sec = (video_fps_ > 0.0) ? static_cast<int>(frame_count_ / video_fps_) : 0;
    int total_sec = (total_frame_count_ > 0 && video_fps_ > 0.0)
                        ? static_cast<int>(total_frame_count_ / video_fps_) : 0;
    time_display_->setText(QString("%1:%2 / %3:%4")
        .arg(elapsed_sec / 60, 2, 10, QChar('0'))
        .arg(elapsed_sec % 60, 2, 10, QChar('0'))
        .arg(total_sec / 60, 2, 10, QChar('0'))
        .arg(total_sec % 60, 2, 10, QChar('0')));

    // Decide whether to schedule next frame
    bool was_step = step_requested_;
    step_requested_ = false;

    if (!was_step && !is_paused_) {
        QTimer::singleShot(0, this, &MainWindow::processNextFrame);
    }
}

void MainWindow::onMatchThresholdChanged(int value) {
    float v = value / 100.0f;
    match_value_label_->setText(QString::number(v, 'f', 2));
    config_.thresholds.match_threshold = v;
    if (pipeline_) {
        pipeline_->UpdateThresholds(config_.thresholds);
    }
}

void MainWindow::onDetectConfidenceChanged(int value) {
    float v = value / 100.0f;
    detect_value_label_->setText(QString::number(v, 'f', 2));
    config_.thresholds.detection_confidence = v;
    if (pipeline_) {
        pipeline_->UpdateThresholds(config_.thresholds);
    }
}

// ============================================================
// Helpers
// ============================================================

void MainWindow::updateStatsDisplay(double fps, int faceCount, bool targetFound) {
    fps_display_->setText(QString::number(fps, 'f', 1));
    face_count_display_->setText(QString::number(faceCount));

    if (targetFound) {
        target_status_display_->setText(QStringLiteral("\u5df2\u53d1\u73b0"));
        target_status_display_->setStyleSheet("color: #4caf50; font-weight: bold;");
    } else {
        target_status_display_->setText(QStringLiteral("\u672a\u53d1\u73b0"));
        target_status_display_->setStyleSheet("color: #888888;");
    }

    status_label_->setText(
        QStringLiteral("\u5e27\u7387: %1 fps | \u4eba\u8138: %2 | \u76ee\u6807: %3")
            .arg(fps, 0, 'f', 1)
            .arg(faceCount)
            .arg(targetFound ? QStringLiteral("\u5df2\u53d1\u73b0") : QStringLiteral("\u672a\u53d1\u73b0")));
}

void MainWindow::updateStateDisplay() {
    bool has_both = !video_path_.empty() && !reference_path_.empty();
    if (!is_running_) {
        play_btn_->setEnabled(has_both);
        pause_btn_->setEnabled(false);
        stop_btn_->setEnabled(false);
        step_btn_->setEnabled(false);
        replay_btn_->setEnabled(has_both && has_finished_once_);
    }
}

void MainWindow::setUiEnabled(bool running) {
    bool has_both = !video_path_.empty() && !reference_path_.empty();
    play_btn_->setEnabled(!running && has_both);
    pause_btn_->setEnabled(running);
    stop_btn_->setEnabled(running);
    step_btn_->setEnabled(running && is_paused_);
    replay_btn_->setEnabled(!running && has_finished_once_);
    open_video_btn_->setEnabled(!running);
    open_ref_btn_->setEnabled(!running);
}

void MainWindow::setFinishedState() {
    bool has_both = !video_path_.empty() && !reference_path_.empty();
    play_btn_->setEnabled(false);
    pause_btn_->setEnabled(false);
    stop_btn_->setEnabled(false);
    step_btn_->setEnabled(false);
    replay_btn_->setEnabled(has_both);
    open_video_btn_->setEnabled(true);
    open_ref_btn_->setEnabled(true);
}

}  // namespace omniface::ui
