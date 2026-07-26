#include "omniface/ui/main_window.h"

#include <chrono>
#include <cmath>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QUuid>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>

#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/align.h"
#include "omniface/core/recognition/face_recognizer.h"

#include <QProgressBar>

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

// ============================================================
// UI Setup
// ============================================================

static QLabel* makeSectionHeader(const QString& text) {
    auto* label = new QLabel(text);
    label->setStyleSheet("font-weight: bold; color: #1565c0; font-size: 13px; padding: 2px 0;");
    return label;
}

void MainWindow::setupUI() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* hlayout = new QHBoxLayout(central);
    hlayout->setContentsMargins(8, 8, 8, 8);
    hlayout->setSpacing(8);

    // ============================================================
    // Section 1: 文件选择
    // ============================================================
    auto* file_section = new QWidget;
    auto* file_layout = new QVBoxLayout(file_section);
    file_layout->setContentsMargins(4, 4, 4, 4);
    file_layout->setSpacing(6);

    file_layout->addWidget(makeSectionHeader(QStringLiteral("\u6587\u4ef6\u9009\u62e9")));

    auto* video_btn_row = new QHBoxLayout;
    open_video_btn_ = new QPushButton(QStringLiteral("\u4e0a\u4f20\u89c6\u9891"));
    connect(open_video_btn_, &QPushButton::clicked, this, &MainWindow::onOpenVideo);
    video_btn_row->addWidget(open_video_btn_);

    open_saved_video_btn_ = new QPushButton(QStringLiteral("\u9009\u62e9\u89c6\u9891"));
    connect(open_saved_video_btn_, &QPushButton::clicked, this, &MainWindow::onOpenSavedVideo);
    video_btn_row->addWidget(open_saved_video_btn_);
    file_layout->addLayout(video_btn_row);

    video_preview_ = new VideoPlayerWidget;
    video_preview_->setObjectName("videoPreview");
    video_preview_->setMinimumHeight(140);
    video_preview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    file_layout->addWidget(video_preview_, 1);

    auto* img_btn_row = new QHBoxLayout;
    open_ref_btn_ = new QPushButton(QStringLiteral("\u4e0a\u4f20\u56fe\u7247"));
    connect(open_ref_btn_, &QPushButton::clicked, this, &MainWindow::onOpenReference);
    img_btn_row->addWidget(open_ref_btn_);

    open_saved_ref_btn_ = new QPushButton(QStringLiteral("\u9009\u62e9\u56fe\u7247"));
    connect(open_saved_ref_btn_, &QPushButton::clicked, this, &MainWindow::onOpenSavedImage);
    img_btn_row->addWidget(open_saved_ref_btn_);
    file_layout->addLayout(img_btn_row);

    auto* ref_layout = new QHBoxLayout;
    ref_original_thumbnail_ = new QLabel;
    ref_original_thumbnail_->setObjectName("refThumbnail");
    ref_original_thumbnail_->setFixedSize(130, 130);
    ref_original_thumbnail_->setAlignment(Qt::AlignCenter);
    ref_original_thumbnail_->setText(QStringLiteral("\u539f\u59cb\u56fe\u50cf"));
    ref_layout->addWidget(ref_original_thumbnail_);

    ref_detected_thumbnail_ = new QLabel;
    ref_detected_thumbnail_->setObjectName("refThumbnail");
    ref_detected_thumbnail_->setFixedSize(130, 130);
    ref_detected_thumbnail_->setAlignment(Qt::AlignCenter);
    ref_detected_thumbnail_->setText(QStringLiteral("\u68c0\u6d4b\u4eba\u8138"));
    ref_layout->addWidget(ref_detected_thumbnail_);

    file_layout->addLayout(ref_layout);

    // ============================================================
    // Section 2: 运行控制
    // ============================================================
    auto* ctrl_section = new QWidget;
    auto* ctrl_layout = new QVBoxLayout(ctrl_section);
    ctrl_layout->setContentsMargins(4, 4, 4, 4);
    ctrl_layout->setSpacing(6);

    ctrl_layout->addWidget(makeSectionHeader(QStringLiteral("\u8fd0\u884c\u63a7\u5236")));

    auto* btn_row1 = new QHBoxLayout;
    play_btn_ = new QPushButton(QStringLiteral("\u25b6 \u5f00\u59cb\u63d0\u53d6"));
    play_btn_->setObjectName("playBtn");
    play_btn_->setEnabled(false);
    connect(play_btn_, &QPushButton::clicked, this, &MainWindow::onPlay);
    btn_row1->addWidget(play_btn_);

    pause_btn_ = new QPushButton(QStringLiteral("\u23f8 \u6682\u505c\u5904\u7406"));
    pause_btn_->setEnabled(false);
    connect(pause_btn_, &QPushButton::clicked, this, &MainWindow::onPause);
    btn_row1->addWidget(pause_btn_);

    stop_btn_ = new QPushButton(QStringLiteral("\u25a0 \u505c\u6b62\u5904\u7406"));
    stop_btn_->setObjectName("stopBtn");
    stop_btn_->setEnabled(false);
    connect(stop_btn_, &QPushButton::clicked, this, &MainWindow::onStop);
    btn_row1->addWidget(stop_btn_);

    ctrl_layout->addLayout(btn_row1);

    auto* btn_row2 = new QHBoxLayout;
    step_btn_ = new QPushButton(QStringLiteral("\u23ed \u9010\u5e27\u5904\u7406"));
    step_btn_->setEnabled(false);
    connect(step_btn_, &QPushButton::clicked, this, &MainWindow::onStep);
    btn_row2->addWidget(step_btn_);

    replay_btn_ = new QPushButton(QStringLiteral("\u21ba \u91cd\u65b0\u63d0\u53d6"));
    replay_btn_->setObjectName("replayBtn");
    replay_btn_->setEnabled(false);
    connect(replay_btn_, &QPushButton::clicked, this, &MainWindow::onReplay);
    btn_row2->addWidget(replay_btn_);

    recognize_only_btn_ = new QPushButton(QStringLiteral("\U0001F50D \u5f00\u59cb\u8bc6\u522b\u76ee\u6807"));
    recognize_only_btn_->setEnabled(false);
    connect(recognize_only_btn_, &QPushButton::clicked, this, &MainWindow::onRecognizeOnly);
    btn_row2->addWidget(recognize_only_btn_);

    ctrl_layout->addLayout(btn_row2);

    // ============================================================
    // Section 3: 实时统计
    // ============================================================
    auto* stats_section = new QWidget;
    auto* stats_layout = new QVBoxLayout(stats_section);
    stats_layout->setContentsMargins(4, 4, 4, 4);
    stats_layout->setSpacing(4);

    stats_layout->addWidget(makeSectionHeader(QStringLiteral("\u5b9e\u65f6\u7edf\u8ba1")));

    auto* stats_form = new QFormLayout;
    fps_display_ = new QLabel("--");
    stats_form->addRow(QStringLiteral("\u5904\u7406\u5e27\u7387:"), fps_display_);
    face_count_display_ = new QLabel("--");
    stats_form->addRow(QStringLiteral("\u68c0\u6d4b\u4eba\u8138:"), face_count_display_);
    target_status_display_ = new QLabel(QStringLiteral("\u672a\u5f00\u59cb"));
    stats_form->addRow(QStringLiteral("\u76ee\u6807\u72b6\u6001:"), target_status_display_);
    stats_layout->addLayout(stats_form);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #d0d0d0;");
    stats_layout->addWidget(sep);

    counter_summary_ = new QLabel;
    counter_summary_->setWordWrap(true);
    counter_summary_->setTextFormat(Qt::RichText);
    counter_summary_->setText(QStringLiteral("<span style='color:#999999'>\u5c1a\u672a\u5f00\u59cb\u5904\u7406</span>"));
    stats_layout->addWidget(counter_summary_);

    // ============================================================
    // Left splitter: 文件选择 / 运行控制 / 实时统计
    // ============================================================
    auto* left_splitter = new QSplitter(Qt::Vertical);
    left_splitter->setHandleWidth(3);
    left_splitter->setChildrenCollapsible(false);
    left_splitter->addWidget(file_section);
    left_splitter->addWidget(ctrl_section);
    left_splitter->addWidget(stats_section);
    left_splitter->setStretchFactor(0, 1);
    left_splitter->setStretchFactor(1, 0);
    left_splitter->setStretchFactor(2, 0);
    left_splitter->setSizes({300, 100, 180});

    // ============================================================
    // Right side: 分析视频显示 + progress + FPS panel
    // ============================================================

    // Video section: display + progress bar
    auto* video_section = new QWidget;
    auto* video_layout = new QVBoxLayout(video_section);
    video_layout->setContentsMargins(0, 0, 0, 0);
    video_layout->setSpacing(2);

    video_label_ = new QLabel(QStringLiteral("\u8bf7\u9009\u62e9\u89c6\u9891\u6587\u4ef6\u548c\u76ee\u6807\u4eba\u7269\u56fe\u50cf\uff0c\u7136\u540e\u70b9\u51fb\u5f00\u59cb\u5904\u7406"));
    video_label_->setObjectName("videoLabel");
    video_label_->setAlignment(Qt::AlignCenter);
    video_label_->setMinimumSize(320, 240);
    video_layout->addWidget(video_label_, 1);

    processing_progress_ = new QProgressBar;
    processing_progress_->setObjectName("processingProgress");
    processing_progress_->setRange(0, 1000);
    processing_progress_->setValue(0);
    processing_progress_->setTextVisible(false);
    processing_progress_->setFixedHeight(4);
    video_layout->addWidget(processing_progress_);

    // FPS panel
    fps_panel_ = new FpsPanel;

    // Right vertical splitter: video section / FPS panel
    auto* right_splitter = new QSplitter(Qt::Vertical);
    right_splitter->setHandleWidth(3);
    right_splitter->setChildrenCollapsible(false);
    right_splitter->addWidget(video_section);
    right_splitter->addWidget(fps_panel_);
    right_splitter->setStretchFactor(0, 1);
    right_splitter->setStretchFactor(1, 0);
    right_splitter->setSizes({500, 34});

    // ============================================================
    // Main horizontal splitter: left / right
    // ============================================================
    auto* main_splitter = new QSplitter(Qt::Horizontal);
    main_splitter->setHandleWidth(4);
    main_splitter->setChildrenCollapsible(false);
    main_splitter->addWidget(left_splitter);
    main_splitter->addWidget(right_splitter);
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setSizes({320, 700});
    hlayout->addWidget(main_splitter);

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
        "QMainWindow { background-color: #f5f5f5; }"
        "QWidget { color: #333333; font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif; font-size: 13px; }"
        "QPushButton { background-color: #ffffff; border: 1px solid #c0c0c0; border-radius: 4px; padding: 6px 14px; color: #333333; min-height: 28px; }"
        "QPushButton:hover { background-color: #e8e8e8; border-color: #a0a0a0; }"
        "QPushButton:pressed { background-color: #d0d0d0; }"
        "QPushButton:disabled { background-color: #f0f0f0; color: #b0b0b0; border-color: #d8d8d8; }"
        "QPushButton#playBtn { background-color: #e8f5e9; border-color: #4caf50; color: #2e7d32; }"
        "QPushButton#playBtn:hover { background-color: #c8e6c9; }"
        "QPushButton#playBtn:disabled { background-color: #f0f0f0; color: #b0b0b0; border-color: #d8d8d8; }"
        "QPushButton#stopBtn { background-color: #ffebee; border-color: #e53935; color: #c62828; }"
        "QPushButton#stopBtn:hover { background-color: #ffcdd2; }"
        "QPushButton#stopBtn:disabled { background-color: #f0f0f0; color: #b0b0b0; border-color: #d8d8d8; }"
        "QPushButton#replayBtn { background-color: #e3f2fd; border-color: #42a5f5; color: #1565c0; }"
        "QPushButton#replayBtn:hover { background-color: #bbdefb; }"
        "QPushButton#replayBtn:disabled { background-color: #f0f0f0; color: #b0b0b0; border-color: #d8d8d8; }"
        "QLabel { background: transparent; border: none; }"
        "QLabel#videoLabel { background-color: #fafafa; border: 2px solid #d0d0d0; border-radius: 8px; color: #999999; font-size: 15px; }"
        "QLabel#refThumbnail { background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px; color: #999999; font-size: 12px; }"
        "QStatusBar { background-color: #f0f0f0; border-top: 1px solid #d0d0d0; color: #666666; }"
        "QScrollArea { border: none; background: transparent; }"
        "QMenuBar { background-color: #fafafa; border-bottom: 1px solid #e0e0e0; padding: 2px; }"
        "QMenuBar::item { padding: 4px 12px; border-radius: 4px; }"
        "QMenuBar::item:selected { background-color: #e0e0e0; }"
        "QMenu { background-color: #ffffff; border: 1px solid #d0d0d0; padding: 4px; }"
        "QMenu::item { padding: 6px 28px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #e3f2fd; }"
        "QMenu::separator { height: 1px; background: #e0e0e0; margin: 4px 8px; }"
        "QProgressBar#processingProgress { background-color: #e0e0e0; border: none; border-radius: 2px; }"
        "QProgressBar#processingProgress::chunk { background-color: #42a5f5; border-radius: 2px; }"
        "QSplitter::handle { background-color: #d0d0d0; }"
        "QSplitter::handle:hover { background-color: #b0b0b0; }"
        "QSplitter::handle:horizontal { width: 4px; }"
        "QSplitter::handle:vertical { height: 3px; }"
    ));
}

// ============================================================
// Helpers
// ============================================================

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
            det_cfg.confidence_threshold = 0.5f;
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

    status_label_->setText(QStringLiteral("\u6b63\u5728\u590d\u5236\u89c6\u9891\u6587\u4ef6..."));
    QApplication::processEvents();

    QString app_dir = QCoreApplication::applicationDirPath();
    QString videos_dir = app_dir + "/data/upload/video";
    QDir().mkpath(videos_dir);

    QFileInfo file_info(path);
    QString ext = file_info.suffix();
    QString saved_name = QUuid::createUuid().toString(QUuid::WithoutBraces) + "." + ext;
    QString saved_path = videos_dir + "/" + saved_name;

    if (QFile::copy(path, saved_path)) {
        video_path_ = saved_path.toStdString();
        spdlog::info("Video copied to: {}", video_path_);
        status_label_->setText(QStringLiteral("\u89c6\u9891\u4e0a\u4f20\u6210\u529f: %1")
                                  .arg(file_info.fileName()));
    } else {
        video_path_ = path.toStdString();
        spdlog::warn("Failed to copy video, using original path: {}", video_path_);
        status_label_->setText(QStringLiteral("\u89c6\u9891\u590d\u5236\u5931\u8d25\uff0c\u4f7f\u7528\u539f\u59cb\u8def\u5f84"));
    }

    video_preview_->setVideoPath(video_path_);
    setUiEnabled(false);
}

void MainWindow::onOpenReference() {
    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("\u9009\u62e9\u76ee\u6807\u4eba\u7269\u56fe\u50cf"),
        "",
        QStringLiteral("\u56fe\u50cf\u6587\u4ef6 (*.jpg *.jpeg *.png *.bmp);;\u6240\u6709\u6587\u4ef6 (*)"));
    if (path.isEmpty()) return;

    status_label_->setText(QStringLiteral("\u6b63\u5728\u590d\u5236\u56fe\u50cf\u6587\u4ef6..."));
    QApplication::processEvents();

    QString app_dir = QCoreApplication::applicationDirPath();
    QString img_dir = app_dir + "/data/upload/img";
    QDir().mkpath(img_dir);

    QFileInfo file_info(path);
    QString ext = file_info.suffix();
    QString saved_name = QUuid::createUuid().toString(QUuid::WithoutBraces) + "." + ext;
    QString saved_path = img_dir + "/" + saved_name;

    if (QFile::copy(path, saved_path)) {
        reference_path_ = saved_path.toStdString();
        spdlog::info("Image copied to: {}", reference_path_);
    } else {
        reference_path_ = path.toStdString();
        spdlog::warn("Failed to copy image, using original path: {}", reference_path_);
    }

    config_.model.reference_image = reference_path_;
    updateRefThumbnails();

    reference_cached_ = false;
    cached_reference_embedding_ = FaceEmbedding{};
    status_label_->setText(QStringLiteral("\u6b63\u5728\u5904\u7406\u4eba\u8138\u7279\u5f81..."));
    QApplication::processEvents();

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
                        status_label_->setText(QStringLiteral("\u56fe\u50cf\u4e0a\u4f20\u6210\u529f: %1  |  \u4eba\u8138\u5df2\u5904\u7406")
                                                  .arg(file_info.fileName()));
                    }
                }
            } else {
                spdlog::warn("No face detected in reference image");
                status_label_->setText(QStringLiteral("\u56fe\u50cf\u4e0a\u4f20\u6210\u529f: %1  |  \u672a\u68c0\u6d4b\u5230\u4eba\u8138")
                                          .arg(file_info.fileName()));
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to cache reference embedding: {}", e.what());
        status_label_->setText(QStringLiteral("\u4eba\u8138\u7279\u5f81\u5904\u7406\u5931\u8d25"));
    }

    setUiEnabled(false);
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

    recognition_only_ = false;
    config_.behavior.skip_analysis = false;
    config_.model.reference_image = reference_path_;
    config_.video.input_path = video_path_;

    try {
        pipeline_ = std::make_unique<Pipeline>(config_);
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
        processing_progress_->setRange(0, total_frame_count_);
        processing_progress_->setValue(0);
    }

    is_running_ = true;
    is_paused_ = false;
    step_requested_ = false;
    has_finished_once_ = false;
    frame_count_ = 0;
    fps_frame_count_ = 0;
    current_fps_ = 0.0;
    last_fps_time_ = std::chrono::steady_clock::now();

    last_dashboard_time_ = last_fps_time_;
    total_faces_detected_ = 0;
    total_matches_ = 0;
    total_openface_analyses_ = 0;
    run_start_time_ = last_fps_time_;

    DashboardData dd;
    fps_panel_->updateData(dd);
    counter_summary_->setText(QStringLiteral("<span style='color:#999999'>\u5c1a\u672a\u5f00\u59cb\u5904\u7406</span>"));

    setUiEnabled(true);
    status_label_->setText(QStringLiteral("\u6b63\u5728\u5904\u7406..."));

    QTimer::singleShot(0, this, &MainWindow::processNextFrame);
}

void MainWindow::onPause() {
    if (!is_running_) return;

    if (is_paused_) {
        is_paused_ = false;
        step_requested_ = false;
        pause_btn_->setText(QStringLiteral("\u23f8 \u6682\u505c\u5904\u7406"));
        step_btn_->setEnabled(false);
        status_label_->setText(QStringLiteral("\u6b63\u5728\u5904\u7406..."));
        QTimer::singleShot(0, this, &MainWindow::processNextFrame);
    } else {
        is_paused_ = true;
        pause_btn_->setText(QStringLiteral("\u25b6 \u7ee7\u7eed\u5904\u7406"));
        step_btn_->setEnabled(true);
        status_label_->setText(QStringLiteral("\u5df2\u6682\u505c"));
    }
}

void MainWindow::onStop() {
    is_running_ = false;
    is_paused_ = false;
    step_requested_ = false;

    pipeline_.reset();

    pause_btn_->setText(QStringLiteral("\u23f8 \u6682\u505c\u5904\u7406"));
    step_btn_->setEnabled(false);
    status_label_->setText(QStringLiteral("\u5df2\u505c\u6b62"));

    processing_progress_->setValue(0);
    processing_progress_->setEnabled(false);

    DashboardData dd;
    fps_panel_->updateData(dd);
    counter_summary_->setText(QStringLiteral("<span style='color:#999999'>\u5c1a\u672a\u5f00\u59cb\u5904\u7406</span>"));

    has_finished_once_ = true;
    setFinishedState();
}

void MainWindow::onStep() {
    if (!is_running_ || !is_paused_) return;
    step_requested_ = true;
    status_label_->setText(QStringLiteral("\u5df2\u6682\u505c  |  \u9010\u5e27\u6a21\u5f0f"));
    QTimer::singleShot(0, this, &MainWindow::processNextFrame);
}

void MainWindow::onReplay() {
    onStop();
    if (recognition_only_) {
        QTimer::singleShot(200, this, &MainWindow::onRecognizeOnly);
    } else {
        QTimer::singleShot(200, this, &MainWindow::onPlay);
    }
}

void MainWindow::onRecognizeOnly() {
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

    recognition_only_ = true;
    config_.behavior.skip_analysis = true;
    config_.model.reference_image = reference_path_;
    config_.video.input_path = video_path_;

    try {
        pipeline_ = std::make_unique<Pipeline>(config_);
        pipeline_->OpenVideo(video_path_);
    } catch (const std::exception& e) {
        QMessageBox::critical(this,
                              QStringLiteral("\u9519\u8bef"),
                              QStringLiteral("\u521d\u59cb\u5316\u7ba1\u7ebf\u5931\u8d25: %1").arg(e.what()));
        config_.behavior.skip_analysis = false;
        return;
    }
    config_.behavior.skip_analysis = false;

    video_fps_ = pipeline_->GetFps();
    total_frame_count_ = pipeline_->GetFrameCount();

    if (total_frame_count_ > 0) {
        processing_progress_->setRange(0, total_frame_count_);
        processing_progress_->setValue(0);
    }

    is_running_ = true;
    is_paused_ = false;
    step_requested_ = false;
    has_finished_once_ = false;
    frame_count_ = 0;
    fps_frame_count_ = 0;
    current_fps_ = 0.0;
    last_fps_time_ = std::chrono::steady_clock::now();

    last_dashboard_time_ = last_fps_time_;
    total_faces_detected_ = 0;
    total_matches_ = 0;
    total_openface_analyses_ = 0;
    run_start_time_ = last_fps_time_;

    DashboardData dd;
    fps_panel_->updateData(dd);
    counter_summary_->setText(QStringLiteral("<span style='color:#999999'>\u5c1a\u672a\u5f00\u59cb\u5904\u7406</span>"));

    setUiEnabled(true);
    status_label_->setText(QStringLiteral("\u6b63\u5728\u8bc6\u522b\u76ee\u6807..."));

    QTimer::singleShot(0, this, &MainWindow::processNextFrame);
}

void MainWindow::processNextFrame() {
    if (!is_running_) return;

    if (is_paused_ && !step_requested_) return;

    cv::Mat frame;
    if (!pipeline_->PollFrame(frame)) {
        if (pipeline_->IsDecodeFinished()) {
            is_running_ = false;
            is_paused_ = false;
            has_finished_once_ = true;
            pipeline_.reset();
            setFinishedState();
            status_label_->setText(QStringLiteral("\u5904\u7406\u5b8c\u6210"));
            if (total_frame_count_ > 0) {
                processing_progress_->setValue(total_frame_count_);
            }
            return;
        }
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

    QImage qimg = matToQImage(result.annotated_frame);
    QPixmap pixmap = QPixmap::fromImage(qimg);
    if (!pixmap.isNull()) {
        pixmap = pixmap.scaled(video_label_->size(), Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
        video_label_->setPixmap(pixmap);
    }

    bool target_found = false;
    for (const auto& mr : result.match_results) {
        if (mr.is_match) { target_found = true; break; }
    }

    updateStatsDisplay(current_fps_, result.face_count, target_found);

    total_faces_detected_ += result.face_count;
    if (target_found) total_matches_++;
    if (result.analysis.valid) total_openface_analyses_++;

    double dash_elapsed = std::chrono::duration<double>(now - last_dashboard_time_).count();
    if (dash_elapsed >= 0.5) {
        last_dashboard_time_ = now;

        auto run_elapsed = std::chrono::duration<double>(now - run_start_time_).count();
        int64_t au_pts = total_openface_analyses_ * 17;
        int64_t lm_pts = total_openface_analyses_ * 68 * 2;
        int64_t pose_recs = total_openface_analyses_ * 6;
        int64_t gaze_recs = total_openface_analyses_ * 2;
        double vol_mb = (total_openface_analyses_ * 644.0 + total_matches_ * 2048.0) / (1024.0 * 1024.0);

        double rec_fps = (result.recognition_latency_ms > 0.0)
            ? 1000.0 / result.recognition_latency_ms : 0.0;
        double ana_fps = (result.analysis_latency_ms > 0.0)
            ? 1000.0 / result.analysis_latency_ms : 0.0;

        DashboardData dd;
        dd.pipeline_fps = current_fps_;
        dd.recognition_fps = rec_fps;
        dd.analysis_fps = ana_fps;
        dd.analysis_running = (total_openface_analyses_ > 0 && result.analysis.valid);
        dd.processed_frames = frame_count_;
        dd.face_detections = total_faces_detected_;
        dd.target_matches = total_matches_;
        dd.openface_analyses = total_openface_analyses_;
        dd.au_data_points = au_pts;
        dd.landmark_points = lm_pts;
        dd.pose_records = pose_recs;
        dd.gaze_records = gaze_recs;
        dd.data_volume_mb = vol_mb;
        dd.elapsed_seconds = static_cast<int64_t>(run_elapsed);
        fps_panel_->updateData(dd);

        auto fmtMb = [](double mb) {
            if (mb < 1.0) return QString::number(mb * 1024.0, 'f', 0) + " KB";
            return QString::number(mb, 'f', 1) + " MB";
        };
        auto fmtTime = [](int64_t sec) {
            int h = static_cast<int>(sec / 3600);
            int m = static_cast<int>((sec % 3600) / 60);
            int s = static_cast<int>(sec % 60);
            return QString("%1:%2:%3").arg(h, 2, 10, QChar('0'))
                                      .arg(m, 2, 10, QChar('0'))
                                      .arg(s, 2, 10, QChar('0'));
        };

        counter_summary_->setText(QStringLiteral(
            "<span style='color:#555555'>"
            "\u5df2\u5904\u7406: %1 \u5e27  |  \u4eba\u8138\u68c0\u6d4b: %2 \u6b21  |  \u76ee\u6807\u5339\u914d: %3 \u6b21  |  OpenFace: %4 \u5e27<br>"
            "AU\u6570\u636e: %5 \u70b9  |  \u5173\u952e\u70b9: %6 \u70b9  |  \u59ff\u6001: %7 \u6761  |  \u89c6\u7ebf: %8 \u6761  |  "
            "\u6570\u636e\u91cf: %9  |  \u8fd0\u884c: %10"
            "</span>")
            .arg(frame_count_)
            .arg(total_faces_detected_)
            .arg(total_matches_)
            .arg(total_openface_analyses_)
            .arg(au_pts)
            .arg(lm_pts)
            .arg(pose_recs)
            .arg(gaze_recs)
            .arg(fmtMb(vol_mb))
            .arg(fmtTime(dd.elapsed_seconds)));
    }

    if (total_frame_count_ > 0) {
        processing_progress_->setValue(frame_count_);
    }

    bool was_step = step_requested_;
    step_requested_ = false;

    if (!was_step && !is_paused_) {
        QTimer::singleShot(0, this, &MainWindow::processNextFrame);
    }
}

void MainWindow::updateStatsDisplay(double fps, int faceCount, bool targetFound) {
    fps_display_->setText(QString::number(fps, 'f', 1));
    face_count_display_->setText(QString::number(faceCount));

    if (targetFound) {
        target_status_display_->setText(QStringLiteral("\u5df2\u53d1\u73b0"));
        target_status_display_->setStyleSheet("color: #2e7d32; font-weight: bold;");
    } else {
        target_status_display_->setText(QStringLiteral("\u672a\u53d1\u73b0"));
        target_status_display_->setStyleSheet("color: #999999;");
    }

    status_label_->setText(
        QStringLiteral("\u5e27\u7387: %1 fps | \u4eba\u8138: %2 | \u76ee\u6807: %3")
            .arg(fps, 0, 'f', 1)
            .arg(faceCount)
            .arg(targetFound ? QStringLiteral("\u5df2\u53d1\u73b0") : QStringLiteral("\u672a\u53d1\u73b0")));
}

void MainWindow::setUiEnabled(bool running) {
    bool has_both = !video_path_.empty() && !reference_path_.empty();
    play_btn_->setEnabled(!running && has_both);
    pause_btn_->setEnabled(running);
    stop_btn_->setEnabled(running);
    step_btn_->setEnabled(running && is_paused_);
    replay_btn_->setEnabled(!running && has_finished_once_);
    recognize_only_btn_->setEnabled(!running && has_both);
    open_video_btn_->setEnabled(!running);
    open_saved_video_btn_->setEnabled(!running);
    open_ref_btn_->setEnabled(!running);
    open_saved_ref_btn_->setEnabled(!running);
}

void MainWindow::setFinishedState() {
    bool has_both = !video_path_.empty() && !reference_path_.empty();
    play_btn_->setEnabled(false);
    pause_btn_->setEnabled(false);
    stop_btn_->setEnabled(false);
    step_btn_->setEnabled(false);
    play_btn_->setEnabled(has_both);
    replay_btn_->setEnabled(has_both);
    recognize_only_btn_->setEnabled(has_both);
    open_video_btn_->setEnabled(true);
    open_saved_video_btn_->setEnabled(true);
    open_ref_btn_->setEnabled(true);
    open_saved_ref_btn_->setEnabled(true);
}

void MainWindow::onOpenSavedVideo() {
    QString app_dir = QCoreApplication::applicationDirPath();
    QString videos_dir = app_dir + "/data/upload/video";
    QDir().mkpath(videos_dir);

    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("\u9009\u62e9\u5df2\u4fdd\u5b58\u7684\u89c6\u9891"),
        videos_dir,
        QStringLiteral("\u89c6\u9891\u6587\u4ef6 (*.mp4 *.avi *.mkv *.mov);;\u6240\u6709\u6587\u4ef6 (*)"));
    if (path.isEmpty()) return;

    video_path_ = path.toStdString();
    video_preview_->setVideoPath(video_path_);
    setUiEnabled(false);

    QFileInfo fi(path);
    status_label_->setText(QStringLiteral("\u5df2\u9009\u62e9\u89c6\u9891: %1").arg(fi.fileName()));
    spdlog::info("Selected saved video: {}", video_path_);
}

void MainWindow::onOpenSavedImage() {
    QString app_dir = QCoreApplication::applicationDirPath();
    QString img_dir = app_dir + "/data/upload/img";
    QDir().mkpath(img_dir);

    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("\u9009\u62e9\u5df2\u4fdd\u5b58\u7684\u56fe\u7247"),
        img_dir,
        QStringLiteral("\u56fe\u7247\u6587\u4ef6 (*.jpg *.jpeg *.png *.bmp);;\u6240\u6709\u6587\u4ef6 (*)"));
    if (path.isEmpty()) return;

    reference_path_ = path.toStdString();
    config_.model.reference_image = reference_path_;
    updateRefThumbnails();
    setUiEnabled(false);

    // Eager embedding extraction (same as onOpenReference)
    reference_cached_ = false;
    cached_reference_embedding_ = FaceEmbedding{};
    status_label_->setText(QStringLiteral("\u6b63\u5728\u5904\u7406\u4eba\u8138\u7279\u5f81..."));
    QApplication::processEvents();

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
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to extract reference embedding: {}", e.what());
    }

    QFileInfo fi(path);
    status_label_->setText(reference_cached_
        ? QStringLiteral("\u5df2\u9009\u62e9: %1  |  \u4eba\u8138\u5df2\u5904\u7406").arg(fi.fileName())
        : QStringLiteral("\u5df2\u9009\u62e9: %1  |  \u672a\u68c0\u6d4b\u5230\u4eba\u8138").arg(fi.fileName()));

    spdlog::info("Selected saved image: {}", reference_path_);
}

}  // namespace omniface::ui
