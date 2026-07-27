#include "omniface/ui/image_utils.h"
#include "omniface/ui/process_page.h"

#include <chrono>
#include <cmath>
#include <filesystem>

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

#include "omniface/ui/reference_face_service.h"

namespace omniface::ui {

static QLabel* makeSectionHeader(const QString& text) {
    auto* label = new QLabel(text);
    label->setStyleSheet("font-weight: bold; color: #1565c0; font-size: 13px; padding: 2px 0;");
    return label;
}

ProcessPage::ProcessPage(const PipelineConfig& config, QWidget* parent)
    : QWidget(parent), config_(config) {
    setupUI();
}

void ProcessPage::updateConfig(const PipelineConfig& config) {
    config_ = config;
}

void ProcessPage::setupUI() {
    auto* hlayout = new QHBoxLayout(this);
    hlayout->setContentsMargins(8, 8, 8, 8);
    hlayout->setSpacing(8);

    // --- File section ---
    auto* file_section = new QWidget;
    auto* file_layout = new QVBoxLayout(file_section);
    file_layout->setContentsMargins(4, 4, 4, 4);
    file_layout->setSpacing(6);

    file_layout->addWidget(makeSectionHeader(QStringLiteral("\u6587\u4ef6\u9009\u62e9")));

    auto* video_btn_row = new QHBoxLayout;
    open_video_btn_ = new QPushButton(QStringLiteral("\u4e0a\u4f20\u89c6\u9891"));
    connect(open_video_btn_, &QPushButton::clicked, this, &ProcessPage::onOpenVideo);
    video_btn_row->addWidget(open_video_btn_);

    open_saved_video_btn_ = new QPushButton(QStringLiteral("\u9009\u62e9\u89c6\u9891"));
    connect(open_saved_video_btn_, &QPushButton::clicked, this, &ProcessPage::onOpenSavedVideo);
    video_btn_row->addWidget(open_saved_video_btn_);
    file_layout->addLayout(video_btn_row);

    video_preview_ = new VideoPlayerWidget;
    video_preview_->setObjectName("videoPreview");
    video_preview_->setMinimumHeight(140);
    video_preview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    file_layout->addWidget(video_preview_, 1);

    auto* img_btn_row = new QHBoxLayout;
    open_ref_btn_ = new QPushButton(QStringLiteral("\u4e0a\u4f20\u56fe\u7247"));
    connect(open_ref_btn_, &QPushButton::clicked, this, &ProcessPage::onOpenReference);
    img_btn_row->addWidget(open_ref_btn_);

    open_saved_ref_btn_ = new QPushButton(QStringLiteral("\u9009\u62e9\u56fe\u7247"));
    connect(open_saved_ref_btn_, &QPushButton::clicked, this, &ProcessPage::onOpenSavedImage);
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

    // --- Control section ---
    auto* ctrl_section = new QWidget;
    auto* ctrl_layout = new QVBoxLayout(ctrl_section);
    ctrl_layout->setContentsMargins(4, 4, 4, 4);
    ctrl_layout->setSpacing(6);

    ctrl_layout->addWidget(makeSectionHeader(QStringLiteral("\u8fd0\u884c\u63a7\u5236")));

    auto* btn_row1 = new QHBoxLayout;
    play_btn_ = new QPushButton(QStringLiteral("\u25b6 \u5f00\u59cb\u63d0\u53d6"));
    play_btn_->setObjectName("playBtn");
    play_btn_->setEnabled(false);
    connect(play_btn_, &QPushButton::clicked, this, &ProcessPage::onPlay);
    btn_row1->addWidget(play_btn_);

    pause_btn_ = new QPushButton(QStringLiteral("\u23f8 \u6682\u505c\u5904\u7406"));
    pause_btn_->setEnabled(false);
    connect(pause_btn_, &QPushButton::clicked, this, &ProcessPage::onPause);
    btn_row1->addWidget(pause_btn_);

    stop_btn_ = new QPushButton(QStringLiteral("\u25a0 \u505c\u6b62\u5904\u7406"));
    stop_btn_->setObjectName("stopBtn");
    stop_btn_->setEnabled(false);
    connect(stop_btn_, &QPushButton::clicked, this, &ProcessPage::onStop);
    btn_row1->addWidget(stop_btn_);
    ctrl_layout->addLayout(btn_row1);

    auto* btn_row2 = new QHBoxLayout;
    step_btn_ = new QPushButton(QStringLiteral("\u23ed \u9010\u5e27\u5904\u7406"));
    step_btn_->setEnabled(false);
    connect(step_btn_, &QPushButton::clicked, this, &ProcessPage::onStep);
    btn_row2->addWidget(step_btn_);

    replay_btn_ = new QPushButton(QStringLiteral("\u21ba \u91cd\u65b0\u63d0\u53d6"));
    replay_btn_->setObjectName("replayBtn");
    replay_btn_->setEnabled(false);
    connect(replay_btn_, &QPushButton::clicked, this, &ProcessPage::onReplay);
    btn_row2->addWidget(replay_btn_);

    recognize_only_btn_ = new QPushButton(QStringLiteral("\U0001F50D \u5f00\u59cb\u8bc6\u522b\u76ee\u6807"));
    recognize_only_btn_->setEnabled(false);
    connect(recognize_only_btn_, &QPushButton::clicked, this, &ProcessPage::onRecognizeOnly);
    btn_row2->addWidget(recognize_only_btn_);
    ctrl_layout->addLayout(btn_row2);

    // --- Stats section ---
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

    // --- Left splitter ---
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

    // --- Video section (right) ---
    auto* video_section = new QWidget;
    auto* video_layout = new QVBoxLayout(video_section);
    video_layout->setContentsMargins(0, 0, 0, 0);
    video_layout->setSpacing(2);

    video_label_ = new QLabel(QStringLiteral("\u8bf7\u9009\u62e9\u89c6\u9891\u6587\u4ef6\u548c\u76ee\u6807\u4eba\u7269\u56fe\u50cf\uff0c\u7136\u540e\u70b9\u51fb\u5f00\u59cb\u5904\u7406"));
    video_label_->setObjectName("videoLabel");
    video_label_->setAlignment(Qt::AlignCenter);
    video_label_->setMinimumSize(320, 240);
    video_layout->addWidget(video_label_, 1);

    completion_panel_ = new QWidget;
    completion_panel_->setObjectName("completionPanel");
    auto* completion_layout = new QHBoxLayout(completion_panel_);
    completion_layout->setContentsMargins(4, 4, 4, 4);
    completion_layout->setSpacing(12);
    completion_label_ = new QLabel(QStringLiteral("\u2714 \u5904\u7406\u5b8c\u6210"));
    completion_label_->setStyleSheet("color: #2e7d32; font-weight: bold; font-size: 14px;");
    completion_layout->addWidget(completion_label_);
    completion_layout->addStretch();
    view_details_btn_ = new QPushButton(QStringLiteral("\u67e5\u770b\u8be6\u60c5"));
    view_details_btn_->setObjectName("viewDetailsBtn");
    view_details_btn_->setStyleSheet(
        "QPushButton#viewDetailsBtn { background-color: #1565c0; color: white; border: none; "
        "border-radius: 4px; padding: 6px 18px; font-weight: bold; }"
        "QPushButton#viewDetailsBtn:hover { background-color: #1976d2; }");
    connect(view_details_btn_, &QPushButton::clicked, this, [this]() {
        if (!state_.last_output_dir.empty())
            emit navigateToDetail(state_.last_output_dir);
    });
    completion_layout->addWidget(view_details_btn_);
    completion_panel_->hide();
    video_layout->addWidget(completion_panel_);

    processing_progress_ = new QProgressBar;
    processing_progress_->setObjectName("processingProgress");
    processing_progress_->setRange(0, 1000);
    processing_progress_->setValue(0);
    processing_progress_->setTextVisible(false);
    processing_progress_->setFixedHeight(4);
    video_layout->addWidget(processing_progress_);

    fps_panel_ = new FpsPanel;

    auto* right_splitter = new QSplitter(Qt::Vertical);
    right_splitter->setHandleWidth(3);
    right_splitter->setChildrenCollapsible(false);
    right_splitter->addWidget(video_section);
    right_splitter->addWidget(fps_panel_);
    right_splitter->setStretchFactor(0, 1);
    right_splitter->setStretchFactor(1, 0);
    right_splitter->setSizes({500, 34});

    auto* main_splitter = new QSplitter(Qt::Horizontal);
    main_splitter->setHandleWidth(4);
    main_splitter->setChildrenCollapsible(false);
    main_splitter->addWidget(left_splitter);
    main_splitter->addWidget(right_splitter);
    main_splitter->setStretchFactor(0, 0);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setSizes({320, 700});
    hlayout->addWidget(main_splitter);
}

QImage ProcessPage::matToQImage(const cv::Mat& bgr) {
    return MatToQImage(bgr, qt_image_cache_);
}

void ProcessPage::setVideoPath(const std::string& path) {
    state_.video_path = path;
    video_preview_->setVideoPath(path);
    setUiEnabled(false);
}

void ProcessPage::setReferencePath(const std::string& path) {
    state_.reference_path = path;
    config_.model.reference_image = path;
    state_.reference_cached = false;
    state_.cached_reference_embedding = FaceEmbedding{};
    updateRefThumbnails();
}

void ProcessPage::updateRefThumbnails() {
    if (state_.reference_path.empty()) return;

    {
        cv::Mat ref_img = cv::imread(state_.reference_path);
        if (!ref_img.empty()) {
            QImage qimg = matToQImage(ref_img);
            QPixmap pixmap = QPixmap::fromImage(qimg);
            if (!pixmap.isNull()) {
                pixmap = pixmap.scaled(ref_original_thumbnail_->size(), Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
                ref_original_thumbnail_->setPixmap(pixmap);
            }
        }
    }

    {
        cv::Mat ref_img = cv::imread(state_.reference_path);
        if (!ref_img.empty()) {
            cv::Mat detected = DrawDetectionsOnImage(ref_img, config_.model.detector_model, config_);
            QImage qimg = matToQImage(detected);
            QPixmap pixmap = QPixmap::fromImage(qimg);
            if (!pixmap.isNull()) {
                pixmap = pixmap.scaled(ref_detected_thumbnail_->size(), Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
                ref_detected_thumbnail_->setPixmap(pixmap);
            }
        }
    }
}

void ProcessPage::onOpenVideo() {
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("\u9009\u62e9\u89c6\u9891\u6587\u4ef6"), "",
        QStringLiteral("\u89c6\u9891\u6587\u4ef6 (*.mp4 *.avi *.mkv *.mov);;\u6240\u6709\u6587\u4ef6 (*)"));
    if (path.isEmpty()) return;

    QFileInfo file_info(path);
    constexpr qint64 kMaxVideoSize = 2LL * 1024 * 1024 * 1024;  // 2 GB
    if (file_info.size() > kMaxVideoSize) {
        QMessageBox::warning(this, QStringLiteral("\u6587\u4ef6\u8fc7\u5927"),
                             QStringLiteral("\u89c6\u9891\u6587\u4ef6\u8d85\u8fc72GB\u9650\u5236\uff0c\u8bf7\u9009\u62e9\u66f4\u5c0f\u7684\u6587\u4ef6\u3002"));
        return;
    }

    // Validate extension against allowlist
    QString ext = file_info.suffix().toLower();
    if (ext != "mp4" && ext != "avi" && ext != "mkv" && ext != "mov") {
        QMessageBox::warning(this, QStringLiteral("\u683c\u5f0f\u9519\u8bef"),
                             QStringLiteral("\u4e0d\u652f\u6301\u7684\u89c6\u9891\u683c\u5f0f\uff0c\u8bf7\u9009\u62e9mp4/avi/mkv/mov\u6587\u4ef6\u3002"));
        return;
    }

    emit statusMessage(QStringLiteral("\u6b63\u5728\u590d\u5236\u89c6\u9891\u6587\u4ef6..."));
    QApplication::processEvents();

    QString app_dir = QCoreApplication::applicationDirPath();
    QString videos_dir = app_dir + "/data/upload/video";
    QDir().mkpath(videos_dir);

    QString saved_name = QUuid::createUuid().toString(QUuid::WithoutBraces) + "." + ext;
    QString saved_path = videos_dir + "/" + saved_name;

    if (QFile::copy(path, saved_path)) {
        state_.video_path = saved_path.toStdString();
        spdlog::info("Video copied to: {}", state_.video_path);
        emit statusMessage(QStringLiteral("\u89c6\u9891\u4e0a\u4f20\u6210\u529f: %1").arg(file_info.fileName()));
    } else {
        state_.video_path = path.toStdString();
        spdlog::warn("Failed to copy video, using original path: {}", state_.video_path);
        emit statusMessage(QStringLiteral("\u89c6\u9891\u590d\u5236\u5931\u8d25\uff0c\u4f7f\u7528\u539f\u59cb\u8def\u5f84"));
    }

    video_preview_->setVideoPath(state_.video_path);
    setUiEnabled(false);
}

void ProcessPage::onOpenReference() {
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("\u9009\u62e9\u76ee\u6807\u4eba\u7269\u56fe\u50cf"), "",
        QStringLiteral("\u56fe\u50cf\u6587\u4ef6 (*.jpg *.jpeg *.png *.bmp);;\u6240\u6709\u6587\u4ef6 (*)"));
    if (path.isEmpty()) return;

    QFileInfo file_info(path);
    constexpr qint64 kMaxImageSize = 50LL * 1024 * 1024;  // 50 MB
    if (file_info.size() > kMaxImageSize) {
        QMessageBox::warning(this, QStringLiteral("\u6587\u4ef6\u8fc7\u5927"),
                             QStringLiteral("\u56fe\u50cf\u6587\u4ef6\u8d85\u8fc750MB\u9650\u5236\uff0c\u8bf7\u9009\u62e9\u66f4\u5c0f\u7684\u6587\u4ef6\u3002"));
        return;
    }

    // Validate extension against allowlist
    QString ext = file_info.suffix().toLower();
    if (ext != "jpg" && ext != "jpeg" && ext != "png" && ext != "bmp") {
        QMessageBox::warning(this, QStringLiteral("\u683c\u5f0f\u9519\u8bef"),
                             QStringLiteral("\u4e0d\u652f\u6301\u7684\u56fe\u7247\u683c\u5f0f\uff0c\u8bf7\u9009\u62e9jpg/png/bmp\u6587\u4ef6\u3002"));
        return;
    }

    emit statusMessage(QStringLiteral("\u6b63\u5728\u590d\u5236\u56fe\u50cf\u6587\u4ef6..."));
    QApplication::processEvents();

    QString app_dir = QCoreApplication::applicationDirPath();
    QString img_dir = app_dir + "/data/upload/img";
    QDir().mkpath(img_dir);

    QString saved_name = QUuid::createUuid().toString(QUuid::WithoutBraces) + "." + ext;
    QString saved_path = img_dir + "/" + saved_name;

    if (QFile::copy(path, saved_path)) {
        state_.reference_path = saved_path.toStdString();
        spdlog::info("Image copied to: {}", state_.reference_path);
    } else {
        state_.reference_path = path.toStdString();
        spdlog::warn("Failed to copy image, using original path: {}", state_.reference_path);
    }

    config_.model.reference_image = state_.reference_path;
    updateRefThumbnails();

    state_.reference_cached = false;
    state_.cached_reference_embedding = FaceEmbedding{};
    emit statusMessage(QStringLiteral("\u6b63\u5728\u5904\u7406\u4eba\u8138\u7279\u5f81..."));
    QApplication::processEvents();

    bool success = false;
    state_.cached_reference_embedding = ExtractReferenceFace(state_.reference_path, config_, success);
    state_.reference_cached = success;

    if (success) {
        emit statusMessage(QStringLiteral("\u56fe\u50cf\u4e0a\u4f20\u6210\u529f: %1  |  \u4eba\u8138\u5df2\u5904\u7406").arg(file_info.fileName()));
    } else {
        emit statusMessage(QStringLiteral("\u56fe\u50cf\u4e0a\u4f20\u6210\u529f: %1  |  \u672a\u68c0\u6d4b\u5230\u4eba\u8138").arg(file_info.fileName()));
    }

    setUiEnabled(false);
}

void ProcessPage::onOpenSavedVideo() {
    QString app_dir = QCoreApplication::applicationDirPath();
    QString videos_dir = app_dir + "/data/upload/video";
    QDir().mkpath(videos_dir);

    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("\u9009\u62e9\u5df2\u4fdd\u5b58\u7684\u89c6\u9891"), videos_dir,
        QStringLiteral("\u89c6\u9891\u6587\u4ef6 (*.mp4 *.avi *.mkv *.mov);;\u6240\u6709\u6587\u4ef6 (*)"));
    if (path.isEmpty()) return;

    state_.video_path = path.toStdString();
    video_preview_->setVideoPath(state_.video_path);
    setUiEnabled(false);

    QFileInfo fi(path);
    emit statusMessage(QStringLiteral("\u5df2\u9009\u62e9\u89c6\u9891: %1").arg(fi.fileName()));
    spdlog::info("Selected saved video: {}", state_.video_path);
}

void ProcessPage::onOpenSavedImage() {
    QString app_dir = QCoreApplication::applicationDirPath();
    QString img_dir = app_dir + "/data/upload/img";
    QDir().mkpath(img_dir);

    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("\u9009\u62e9\u5df2\u4fdd\u5b58\u7684\u56fe\u7247"), img_dir,
        QStringLiteral("\u56fe\u7247\u6587\u4ef6 (*.jpg *.jpeg *.png *.bmp);;\u6240\u6709\u6587\u4ef6 (*)"));
    if (path.isEmpty()) return;

    setReferencePath(path.toStdString());

    emit statusMessage(QStringLiteral("\u6b63\u5728\u5904\u7406\u4eba\u8138\u7279\u5f81..."));
    QApplication::processEvents();

    bool success = false;
    state_.cached_reference_embedding = ExtractReferenceFace(state_.reference_path, config_, success);
    state_.reference_cached = success;

    QFileInfo fi(path);
    emit statusMessage(success
        ? QStringLiteral("\u5df2\u9009\u62e9: %1  |  \u4eba\u8138\u5df2\u5904\u7406").arg(fi.fileName())
        : QStringLiteral("\u5df2\u9009\u62e9: %1  |  \u672a\u68c0\u6d4b\u5230\u4eba\u8138").arg(fi.fileName()));

    setUiEnabled(false);
}

void ProcessPage::onPlay() {
    if (state_.video_path.empty()) {
        QMessageBox::warning(this, QStringLiteral("\u9519\u8bef"),
                             QStringLiteral("\u8bf7\u5148\u9009\u62e9\u4e00\u4e2a\u89c6\u9891\u6587\u4ef6\u3002"));
        return;
    }
    if (state_.reference_path.empty()) {
        QMessageBox::warning(this, QStringLiteral("\u9519\u8bef"),
                             QStringLiteral("\u8bf7\u5148\u9009\u62e9\u76ee\u6807\u4eba\u7269\u56fe\u50cf\u3002"));
        return;
    }
    if (!std::filesystem::exists(state_.video_path)) {
        QMessageBox::warning(this, QStringLiteral("\u9519\u8bef"),
                             QStringLiteral("\u89c6\u9891\u6587\u4ef6\u5df2\u4e0d\u5b58\u5728\uff0c\u8bf7\u91cd\u65b0\u9009\u62e9\u3002"));
        state_.video_path.clear();
        return;
    }
    if (!state_.reference_path.empty() && !std::filesystem::exists(state_.reference_path)) {
        QMessageBox::warning(this, QStringLiteral("\u9519\u8bef"),
                             QStringLiteral("\u53c2\u8003\u56fe\u7247\u5df2\u4e0d\u5b58\u5728\uff0c\u8bf7\u91cd\u65b0\u9009\u62e9\u3002"));
        state_.reference_path.clear();
        return;
    }

    video_preview_->pause();
    state_.recognition_only_ = false;
    config_.behavior.skip_analysis = false;
    config_.model.reference_image = state_.reference_path;
    config_.video.input_path = state_.video_path;

    try {
        pipeline_ = std::make_unique<Pipeline>(config_);
        pipeline_->OpenVideo(state_.video_path);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, QStringLiteral("\u9519\u8bef"),
                              QStringLiteral("\u521d\u59cb\u5316\u7ba1\u7ebf\u5931\u8d25: %1").arg(e.what()));
        return;
    }

    state_.video_fps = pipeline_->GetFps();
    state_.total_frame_count = pipeline_->GetFrameCount();

    if (state_.total_frame_count > 0) {
        processing_progress_->setRange(0, state_.total_frame_count);
        processing_progress_->setValue(0);
    }

    state_.is_running = true;
    state_.is_paused = false;
    state_.step_requested = false;
    state_.has_finished_once = false;
    state_.frame_count = 0;
    state_.fps_frame_count = 0;
    state_.current_fps = 0.0;
    state_.last_fps_time = std::chrono::steady_clock::now();
    state_.last_dashboard_time = state_.last_fps_time;
    state_.total_faces_detected_ = 0;
    state_.total_matches_ = 0;
    state_.total_openface_analyses_ = 0;
    state_.run_start_time_ = state_.last_fps_time;

    DashboardData dd;
    fps_panel_->updateData(dd);
    counter_summary_->setText(QStringLiteral("<span style='color:#999999'>\u5c1a\u672a\u5f00\u59cb\u5904\u7406</span>"));

    emit processingStateChanged(true);
    setUiEnabled(true);
    emit statusMessage(QStringLiteral("\u6b63\u5728\u5904\u7406..."));
    completion_panel_->hide();

    QTimer::singleShot(0, this, &ProcessPage::processNextFrame);
}

void ProcessPage::onPause() {
    if (!state_.is_running) return;

    if (state_.is_paused) {
        state_.is_paused = false;
        state_.step_requested = false;
        pause_btn_->setText(QStringLiteral("\u23f8 \u6682\u505c\u5904\u7406"));
        step_btn_->setEnabled(false);
        emit statusMessage(QStringLiteral("\u6b63\u5728\u5904\u7406..."));
        QTimer::singleShot(0, this, &ProcessPage::processNextFrame);
    } else {
        state_.is_paused = true;
        pause_btn_->setText(QStringLiteral("\u25b6 \u7ee7\u7eed\u5904\u7406"));
        step_btn_->setEnabled(true);
        emit statusMessage(QStringLiteral("\u5df2\u6682\u505c"));
    }
}

void ProcessPage::onStop() {
    state_.is_running = false;
    state_.is_paused = false;
    state_.step_requested = false;
    emit processingStateChanged(false);

    if (pipeline_) state_.last_output_dir = pipeline_->GetOutputDir();
    pipeline_.reset();

    pause_btn_->setText(QStringLiteral("\u23f8 \u6682\u505c\u5904\u7406"));
    step_btn_->setEnabled(false);
    emit statusMessage(QStringLiteral("\u5df2\u505c\u6b62"));

    processing_progress_->setValue(0);
    processing_progress_->setEnabled(false);

    DashboardData dd;
    fps_panel_->updateData(dd);
    counter_summary_->setText(QStringLiteral("<span style='color:#999999'>\u5c1a\u672a\u5f00\u59cb\u5904\u7406</span>"));

    state_.has_finished_once = true;
    setFinishedState();
}

void ProcessPage::onStep() {
    if (!state_.is_running || !state_.is_paused) return;
    state_.step_requested = true;
    emit statusMessage(QStringLiteral("\u5df2\u6682\u505c  |  \u9010\u5e27\u6a21\u5f0f"));
    QTimer::singleShot(0, this, &ProcessPage::processNextFrame);
}

void ProcessPage::onReplay() {
    onStop();
    if (state_.recognition_only_) {
        QTimer::singleShot(200, this, &ProcessPage::onRecognizeOnly);
    } else {
        QTimer::singleShot(200, this, &ProcessPage::onPlay);
    }
}

void ProcessPage::onRecognizeOnly() {
    if (state_.video_path.empty()) {
        QMessageBox::warning(this, QStringLiteral("\u9519\u8bef"),
                             QStringLiteral("\u8bf7\u5148\u9009\u62e9\u4e00\u4e2a\u89c6\u9891\u6587\u4ef6\u3002"));
        return;
    }
    if (state_.reference_path.empty()) {
        QMessageBox::warning(this, QStringLiteral("\u9519\u8bef"),
                             QStringLiteral("\u8bf7\u5148\u9009\u62e9\u76ee\u6807\u4eba\u7269\u56fe\u50cf\u3002"));
        return;
    }

    video_preview_->pause();
    state_.recognition_only_ = true;
    config_.behavior.skip_analysis = true;
    config_.model.reference_image = state_.reference_path;
    config_.video.input_path = state_.video_path;

    try {
        pipeline_ = std::make_unique<Pipeline>(config_);
        pipeline_->OpenVideo(state_.video_path);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, QStringLiteral("\u9519\u8bef"),
                              QStringLiteral("\u521d\u59cb\u5316\u7ba1\u7ebf\u5931\u8d25: %1").arg(e.what()));
        config_.behavior.skip_analysis = false;
        return;
    }
    config_.behavior.skip_analysis = false;

    state_.video_fps = pipeline_->GetFps();
    state_.total_frame_count = pipeline_->GetFrameCount();

    if (state_.total_frame_count > 0) {
        processing_progress_->setRange(0, state_.total_frame_count);
        processing_progress_->setValue(0);
    }

    state_.is_running = true;
    state_.is_paused = false;
    state_.step_requested = false;
    state_.has_finished_once = false;
    state_.frame_count = 0;
    state_.fps_frame_count = 0;
    state_.current_fps = 0.0;
    state_.last_fps_time = std::chrono::steady_clock::now();
    state_.last_dashboard_time = state_.last_fps_time;
    state_.total_faces_detected_ = 0;
    state_.total_matches_ = 0;
    state_.total_openface_analyses_ = 0;
    state_.run_start_time_ = state_.last_fps_time;

    DashboardData dd;
    fps_panel_->updateData(dd);
    counter_summary_->setText(QStringLiteral("<span style='color:#999999'>\u5c1a\u672a\u5f00\u59cb\u5904\u7406</span>"));

    emit processingStateChanged(true);
    setUiEnabled(true);
    emit statusMessage(QStringLiteral("\u6b63\u5728\u8bc6\u522b\u76ee\u6807..."));
    completion_panel_->hide();

    QTimer::singleShot(0, this, &ProcessPage::processNextFrame);
}

void ProcessPage::processNextFrame() {
    if (!state_.is_running) return;
    if (state_.is_paused && !state_.step_requested) return;

    cv::Mat frame;
    if (!pipeline_->PollFrame(frame)) {
        if (pipeline_->IsDecodeFinished()) {
            state_.is_running = false;
            state_.is_paused = false;
            state_.has_finished_once = true;
            emit processingStateChanged(false);
            state_.last_output_dir = pipeline_->GetOutputDir();
            pipeline_.reset();
            setFinishedState();
            emit statusMessage(QStringLiteral("\u5904\u7406\u5b8c\u6210"));
            if (state_.total_frame_count > 0) {
                processing_progress_->setValue(state_.total_frame_count);
            }
            emit processingFinished(state_.last_output_dir);
            return;
        }
        QTimer::singleShot(2, this, &ProcessPage::processNextFrame);
        return;
    }

    state_.frame_count++;
    state_.fps_frame_count++;

    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - state_.last_fps_time).count();
    if (elapsed >= 1.0) {
        state_.current_fps = state_.fps_frame_count / elapsed;
        state_.fps_frame_count = 0;
        state_.last_fps_time = now;
    }

    auto result = pipeline_->ProcessFrame(frame, state_.frame_count);

    QImage qimg = matToQImage(result.annotated_frame);
    QPixmap pixmap = QPixmap::fromImage(qimg);
    if (!pixmap.isNull()) {
        pixmap = pixmap.scaled(video_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        video_label_->setPixmap(pixmap);
    }

    bool target_found = false;
    for (const auto& mr : result.match_results) {
        if (mr.is_match) { target_found = true; break; }
    }

    updateStatsDisplay(state_.current_fps, result.face_count, target_found);

    state_.total_faces_detected_ += result.face_count;
    if (target_found) state_.total_matches_++;
    if (result.analysis.valid) state_.total_openface_analyses_++;

    double dash_elapsed = std::chrono::duration<double>(now - state_.last_dashboard_time).count();
    if (dash_elapsed >= 0.5) {
        state_.last_dashboard_time = now;

        auto run_elapsed = std::chrono::duration<double>(now - state_.run_start_time_).count();
        int64_t au_pts = state_.total_openface_analyses_ * 17;
        int64_t lm_pts = state_.total_openface_analyses_ * 68 * 2;
        int64_t pose_recs = state_.total_openface_analyses_ * 6;
        int64_t gaze_recs = state_.total_openface_analyses_ * 2;
        double vol_mb = (state_.total_openface_analyses_ * 644.0 + state_.total_matches_ * 2048.0) / (1024.0 * 1024.0);

        double rec_fps = (result.recognition_latency_ms > 0.0)
            ? 1000.0 / result.recognition_latency_ms : 0.0;
        double ana_fps = (result.analysis_latency_ms > 0.0)
            ? 1000.0 / result.analysis_latency_ms : 0.0;

        DashboardData dd;
        dd.pipeline_fps = state_.current_fps;
        dd.recognition_fps = rec_fps;
        dd.analysis_fps = ana_fps;
        dd.analysis_running = (state_.total_openface_analyses_ > 0 && result.analysis.valid);
        dd.processed_frames = state_.frame_count;
        dd.face_detections = state_.total_faces_detected_;
        dd.target_matches = state_.total_matches_;
        dd.openface_analyses = state_.total_openface_analyses_;
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
            .arg(state_.frame_count)
            .arg(state_.total_faces_detected_)
            .arg(state_.total_matches_)
            .arg(state_.total_openface_analyses_)
            .arg(au_pts).arg(lm_pts).arg(pose_recs).arg(gaze_recs)
            .arg(fmtMb(vol_mb)).arg(fmtTime(dd.elapsed_seconds)));
    }

    if (state_.total_frame_count > 0) {
        processing_progress_->setValue(state_.frame_count);
    }

    bool was_step = state_.step_requested;
    state_.step_requested = false;

    if (!was_step && !state_.is_paused) {
        QTimer::singleShot(0, this, &ProcessPage::processNextFrame);
    }
}

void ProcessPage::updateStatsDisplay(double fps, int faceCount, bool targetFound) {
    fps_display_->setText(QString::number(fps, 'f', 1));
    face_count_display_->setText(QString::number(faceCount));

    if (targetFound) {
        target_status_display_->setText(QStringLiteral("\u5df2\u53d1\u73b0"));
        target_status_display_->setStyleSheet("color: #2e7d32; font-weight: bold;");
    } else {
        target_status_display_->setText(QStringLiteral("\u672a\u53d1\u73b0"));
        target_status_display_->setStyleSheet("color: #999999;");
    }

    emit statusMessage(
        QStringLiteral("\u5e27\u7387: %1 fps | \u4eba\u8138: %2 | \u76ee\u6807: %3")
            .arg(fps, 0, 'f', 1).arg(faceCount)
            .arg(targetFound ? QStringLiteral("\u5df2\u53d1\u73b0") : QStringLiteral("\u672a\u53d1\u73b0")));
}

void ProcessPage::setUiEnabled(bool running) {
    bool has_both = !state_.video_path.empty() && !state_.reference_path.empty();
    play_btn_->setEnabled(!running && has_both);
    pause_btn_->setEnabled(running);
    stop_btn_->setEnabled(running);
    step_btn_->setEnabled(running && state_.is_paused);
    replay_btn_->setEnabled(!running && state_.has_finished_once);
    recognize_only_btn_->setEnabled(!running && has_both);
    open_video_btn_->setEnabled(!running);
    open_saved_video_btn_->setEnabled(!running);
    open_ref_btn_->setEnabled(!running);
    open_saved_ref_btn_->setEnabled(!running);
}

void ProcessPage::setFinishedState() {
    bool has_both = !state_.video_path.empty() && !state_.reference_path.empty();
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

    if (!state_.recognition_only_ && state_.has_finished_once) {
        completion_panel_->show();
    }
}

}  // namespace omniface::ui
