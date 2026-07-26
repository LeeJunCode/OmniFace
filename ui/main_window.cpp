#include "omniface/ui/main_window.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

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
#include <QScrollArea>
#include <QStatusBar>
#include <QTimer>

#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/align.h"
#include "omniface/core/recognition/face_recognizer.h"

#include <QProgressBar>

namespace omniface::ui {

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

static QLabel* makeSectionHeader(const QString& text) {
    auto* label = new QLabel(text);
    label->setStyleSheet("font-weight: bold; color: #1565c0; font-size: 13px; padding: 2px 0;");
    return label;
}

void MainWindow::setupUI() {
    central_stack_ = new QStackedWidget;
    setCentralWidget(central_stack_);

    process_page_ = new QWidget;
    auto* hlayout = new QHBoxLayout(process_page_);
    hlayout->setContentsMargins(8, 8, 8, 8);
    hlayout->setSpacing(8);

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
    completion_label_ = new QLabel(QStringLiteral("\u2714 \u5904\u7406\u5b8c\u6bd5"));
    completion_label_->setStyleSheet("color: #2e7d32; font-weight: bold; font-size: 14px;");
    completion_layout->addWidget(completion_label_);
    completion_layout->addStretch();
    view_details_btn_ = new QPushButton(QStringLiteral("\u67e5\u770b\u8be6\u60c5"));
    view_details_btn_->setObjectName("viewDetailsBtn");
    view_details_btn_->setStyleSheet(
        "QPushButton#viewDetailsBtn { background-color: #1565c0; color: white; border: none; "
        "border-radius: 4px; padding: 6px 18px; font-weight: bold; }"
        "QPushButton#viewDetailsBtn:hover { background-color: #1976d2; }");
    connect(view_details_btn_, &QPushButton::clicked, this, &MainWindow::onViewDetails);
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

    status_label_ = new QLabel(QStringLiteral("\u5c31\u7eea"));
    statusBar()->addWidget(status_label_, 1);

    auto* file_menu = menuBar()->addMenu(QStringLiteral("\u6587\u4ef6(&F)"));
    file_menu->addAction(QStringLiteral("\u6253\u5f00\u89c6\u9891(&V)..."), this, &MainWindow::onOpenVideo);
    file_menu->addAction(QStringLiteral("\u6253\u5f00\u76ee\u6807\u4eba\u7269\u56fe\u50cf(&R)..."), this, &MainWindow::onOpenReference);

    auto* nav_menu = menuBar()->addMenu(QStringLiteral("\u5bfc\u822a(&N)"));
    nav_menu->addAction(QStringLiteral("\u8fd4\u56de\u4e3b\u754c\u9762(&M)"), this, &MainWindow::onBackToProcess);

    auto* tool_menu = menuBar()->addMenu(QStringLiteral("\u5de5\u5177(&T)"));
    tool_menu->addAction(QStringLiteral("\u4efb\u52a1\u5386\u53f2(&H)..."), this, &MainWindow::onTaskHistory);
    tool_menu->addSeparator();
    tool_menu->addAction(QStringLiteral("\u67e5\u770b\u4e0a\u4f20\u7684\u89c6\u9891(&V)..."), this, [this]() { showSavedVideosPage(); });
    tool_menu->addAction(QStringLiteral("\u67e5\u770b\u4e0a\u4f20\u7684\u56fe\u7247(&I)..."), this, [this]() { showSavedImagesPage(); });

    auto* help_menu = menuBar()->addMenu(QStringLiteral("\u5e2e\u52a9(&H)"));
    help_menu->addAction(QStringLiteral("\u5173\u4e8e(&A)"), this, [this]() {
        QMessageBox about(this);
        about.setWindowTitle(QStringLiteral("\u5173\u4e8e OmniFace"));
        about.setText(QStringLiteral("OmniFace - \u5b9e\u65f6\u4eba\u8138\u68c0\u6d4b\u3001\u8ddf\u8e2a\u4e0e\u8bc6\u522b\n\n"
                                      "\u57fa\u4e8e SCRFD + ByteTrack + MobileFaceNet"));
        about.setIcon(QMessageBox::Information);
        about.setStyleSheet(
            "QMessageBox { background-color: #ffffff; }"
            "QLabel { color: #333333; font-size: 13px; background: transparent; border: none; }"
            "QPushButton { min-width: 80px; min-height: 28px; background-color: #ffffff; "
            "border: 1px solid #c0c0c0; border-radius: 4px; padding: 6px 14px; color: #333333; }"
            "QPushButton:hover { background-color: #e8e8e8; }");
        about.exec();
    });

    menuBar()->addAction(QStringLiteral("\u9000\u51fa(&X)"), this, &QWidget::close);

    central_stack_->addWidget(process_page_);

    detail_page_ = new QWidget;
    detail_page_->setStyleSheet("background-color: #f5f5f5;");
    {
        auto* dl = new QVBoxLayout(detail_page_);
        dl->setContentsMargins(8, 6, 8, 6);
        dl->setSpacing(6);

        auto* top = new QHBoxLayout;
        auto* back1 = new QPushButton(QStringLiteral("\u2190 \u8fd4\u56de"));
        back1->setFixedWidth(100);
        connect(back1, &QPushButton::clicked, this, &MainWindow::onBackToProcess);
        top->addWidget(back1);
        top->addStretch();
        dl->addLayout(top);

        auto* split = new QSplitter(Qt::Horizontal);

        auto* left = new QWidget;
        auto* ll = new QVBoxLayout(left);
        ll->setContentsMargins(0, 0, 0, 0);
        ll->setSpacing(0);

        detail_video_ = new VideoPlayerWidget;
        detail_video_->setMinimumSize(400, 260);
        ll->addWidget(detail_video_, 1);

        split->addWidget(left);

        auto* right_split = new QSplitter(Qt::Vertical);

        auto panelStyle = QStringLiteral(
            "QFrame { background: #ffffff; border: 1px solid #e0e5ec; border-radius: 6px; }");
        auto panelTitleStyle = QStringLiteral(
            "font-weight: bold; font-size: 13px; color: #333; border: none;");

        auto* panel1 = new QFrame;
        panel1->setStyleSheet(panelStyle);
        auto* p1l = new QVBoxLayout(panel1);
        p1l->setContentsMargins(10, 8, 10, 8);
        p1l->setSpacing(6);

        auto* p1title_row = new QHBoxLayout;
        auto* p1title = new QLabel(QStringLiteral("\u89c6\u9891\u4fe1\u606f"));
        p1title->setStyleSheet(panelTitleStyle);
        p1title_row->addWidget(p1title);
        p1title_row->addStretch();
        auto* detail_del_btn = new QPushButton(QStringLiteral("\u5220\u9664"));
        detail_del_btn->setFixedHeight(26);
        detail_del_btn->setStyleSheet(
            "QPushButton { background-color: #e53935; color: white; border: none; "
            "border-radius: 3px; padding: 4px 12px; font-size: 11px; }"
            "QPushButton:hover { background-color: #c62828; }");
        connect(detail_del_btn, &QPushButton::clicked, this, [this]() {
            onDeleteTask(current_detail_dir_);
        });
        p1title_row->addWidget(detail_del_btn);
        p1l->addLayout(p1title_row);

        detail_info_label_ = new QLabel;
        detail_info_label_->setTextFormat(Qt::RichText);
        detail_info_label_->setWordWrap(true);
        detail_info_label_->setStyleSheet("border: none; font-size: 12px; color: #555; padding: 0;");
        p1l->addWidget(detail_info_label_);

        detail_orig_video_ = new VideoPlayerWidget;
        detail_orig_video_->setMinimumHeight(120);
        detail_orig_video_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        p1l->addWidget(detail_orig_video_, 1);

        right_split->addWidget(panel1);

        auto* panel2 = new QFrame;
        panel2->setStyleSheet(panelStyle);
        auto* p2l = new QVBoxLayout(panel2);
        p2l->setContentsMargins(10, 8, 10, 8);
        p2l->setSpacing(6);

        auto* p2title = new QLabel(QStringLiteral("\u76ee\u6807\u4eba\u8138"));
        p2title->setStyleSheet(panelTitleStyle);
        p2l->addWidget(p2title);

        auto* ref_row = new QHBoxLayout;

        auto* orig_col = new QVBoxLayout;
        detail_ref_thumb_ = new QLabel;
        detail_ref_thumb_->setFixedSize(120, 120);
        detail_ref_thumb_->setAlignment(Qt::AlignCenter);
        detail_ref_thumb_->setStyleSheet(
            "background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px; color: #999; font-size:11px;");
        detail_ref_thumb_->setText(QStringLiteral("\u539f\u59cb\u56fe\u50cf"));
        orig_col->addWidget(detail_ref_thumb_, 0, Qt::AlignHCenter);
        auto* orig_cap = new QLabel(QStringLiteral("\u539f\u59cb\u56fe\u50cf"));
        orig_cap->setAlignment(Qt::AlignCenter);
        orig_cap->setStyleSheet("border: none; font-size: 11px; color: #888;");
        orig_col->addWidget(orig_cap);
        ref_row->addLayout(orig_col);

        auto* det_col = new QVBoxLayout;
        detail_ref_detected_ = new QLabel;
        detail_ref_detected_->setFixedSize(120, 120);
        detail_ref_detected_->setAlignment(Qt::AlignCenter);
        detail_ref_detected_->setStyleSheet(
            "background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px; color: #999; font-size:11px;");
        detail_ref_detected_->setText(QStringLiteral("\u68c0\u6d4b\u4eba\u8138"));
        det_col->addWidget(detail_ref_detected_, 0, Qt::AlignHCenter);
        auto* det_cap = new QLabel(QStringLiteral("\u68c0\u6d4b\u4eba\u8138"));
        det_cap->setAlignment(Qt::AlignCenter);
        det_cap->setStyleSheet("border: none; font-size: 11px; color: #888;");
        det_col->addWidget(det_cap);
        ref_row->addLayout(det_col);

        p2l->addLayout(ref_row);
        p2l->addStretch();

        right_split->addWidget(panel2);

        auto* panel3 = new QFrame;
        panel3->setStyleSheet(panelStyle);
        auto* p3l = new QVBoxLayout(panel3);
        p3l->setContentsMargins(10, 8, 10, 8);
        p3l->setSpacing(4);

        auto* p3title = new QLabel(QStringLiteral("\u5206\u6790\u7ed3\u679c"));
        p3title->setStyleSheet(panelTitleStyle);
        p3l->addWidget(p3title);

        detail_face_thumb_ = new QLabel(QStringLiteral("\u5bf9\u9f50\u4eba\u8138"));
        detail_face_thumb_->setFixedSize(112, 112);
        detail_face_thumb_->setAlignment(Qt::AlignCenter);
        detail_face_thumb_->setStyleSheet(
            "background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px; color: #999;");
        p3l->addWidget(detail_face_thumb_, 0, Qt::AlignHCenter);

        auto* align_cap = new QLabel(QStringLiteral("\u5f53\u524d\u5e27\u5bf9\u9f50\u4eba\u8138"));
        align_cap->setAlignment(Qt::AlignCenter);
        align_cap->setStyleSheet("border: none; font-size: 11px; color: #888;");
        p3l->addWidget(align_cap);

        auto* sep2 = new QFrame;
        sep2->setFrameShape(QFrame::HLine);
        sep2->setStyleSheet("QFrame { border: none; border-top: 1px solid #e8e8e8; background: transparent; }");
        p3l->addWidget(sep2);

        detail_data_label_ = new QLabel(
            QStringLiteral("<span style='color:#999'>\u64ad\u653e\u89c6\u9891\u4ee5\u67e5\u770b\u6bcf\u5e27\u6570\u636e</span>"));
        detail_data_label_->setWordWrap(true);
        detail_data_label_->setTextFormat(Qt::RichText);
        detail_data_label_->setMinimumWidth(280);
        detail_data_label_->setAlignment(Qt::AlignTop);

        auto* sc = new QScrollArea;
        sc->setWidgetResizable(true);
        sc->setWidget(detail_data_label_);
        sc->setStyleSheet("QScrollArea { border: none; background: transparent; }");
        p3l->addWidget(sc, 1);

        right_split->addWidget(panel3);
        right_split->setStretchFactor(0, 0);
        right_split->setStretchFactor(1, 0);
        right_split->setStretchFactor(2, 1);

        split->addWidget(right_split);
        split->setStretchFactor(0, 3);
        split->setStretchFactor(1, 1);
        dl->addWidget(split, 1);
    }
    central_stack_->addWidget(detail_page_);

    history_page_ = new QWidget;
    history_page_->setStyleSheet("background-color: #f5f5f5;");
    {
        auto* hl = new QVBoxLayout(history_page_);
        hl->setContentsMargins(12, 8, 12, 8);
        hl->setSpacing(8);

        auto* top = new QHBoxLayout;
        auto* back2 = new QPushButton(QStringLiteral("\u2190 \u8fd4\u56de"));
        back2->setFixedWidth(100);
        connect(back2, &QPushButton::clicked, this, &MainWindow::onBackToProcess);
        top->addWidget(back2);
        top->addStretch();
        hl->addLayout(top);

        history_scroll_ = new QScrollArea;
        history_scroll_->setWidgetResizable(true);
        history_scroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");

        history_content_ = new QWidget;
        auto* hc_layout = new QVBoxLayout(history_content_);
        hc_layout->setContentsMargins(0, 0, 0, 0);
        hc_layout->setSpacing(8);
        history_scroll_->setWidget(history_content_);
        hl->addWidget(history_scroll_, 1);
    }
    central_stack_->addWidget(history_page_);

    // ============================================================
    // Page 3: saved videos
    // ============================================================
    saved_videos_page_ = new QWidget;
    saved_videos_page_->setStyleSheet("background-color: #f5f5f5;");
    {
        auto* vl = new QVBoxLayout(saved_videos_page_);
        vl->setContentsMargins(12, 8, 12, 8);
        vl->setSpacing(8);

        auto* top = new QHBoxLayout;
        auto* back_btn = new QPushButton(QStringLiteral("\u2190 \u8fd4\u56de"));
        back_btn->setFixedWidth(100);
        connect(back_btn, &QPushButton::clicked, this, &MainWindow::onBackToProcess);
        top->addWidget(back_btn);
        top->addStretch();
        vl->addLayout(top);

        saved_video_preview_ = new VideoPlayerWidget;
        saved_video_preview_->setMinimumHeight(200);
        saved_video_preview_->setMaximumHeight(280);
        saved_video_preview_->hide();
        vl->addWidget(saved_video_preview_);

        saved_videos_scroll_ = new QScrollArea;
        saved_videos_scroll_->setWidgetResizable(true);
        saved_videos_scroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");
        saved_videos_content_ = new QWidget;
        saved_videos_content_->setStyleSheet("background: transparent;");
        auto* vc_layout = new QVBoxLayout(saved_videos_content_);
        vc_layout->setContentsMargins(0, 0, 0, 0);
        vc_layout->setSpacing(8);
        saved_videos_scroll_->setWidget(saved_videos_content_);
        vl->addWidget(saved_videos_scroll_, 1);
    }
    central_stack_->addWidget(saved_videos_page_);

    // ============================================================
    // Page 4: saved images
    // ============================================================
    saved_images_page_ = new QWidget;
    saved_images_page_->setStyleSheet("background-color: #f5f5f5;");
    {
        auto* il = new QVBoxLayout(saved_images_page_);
        il->setContentsMargins(12, 8, 12, 8);
        il->setSpacing(8);

        auto* top = new QHBoxLayout;
        auto* back_btn = new QPushButton(QStringLiteral("\u2190 \u8fd4\u56de"));
        back_btn->setFixedWidth(100);
        connect(back_btn, &QPushButton::clicked, this, &MainWindow::onBackToProcess);
        top->addWidget(back_btn);
        top->addStretch();
        il->addLayout(top);

        saved_images_scroll_ = new QScrollArea;
        saved_images_scroll_->setWidgetResizable(true);
        saved_images_scroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");
        saved_images_content_ = new QWidget;
        saved_images_content_->setStyleSheet("background: transparent;");
        auto* ic_layout = new QVBoxLayout(saved_images_content_);
        ic_layout->setContentsMargins(0, 0, 0, 0);
        ic_layout->setSpacing(8);
        saved_images_scroll_->setWidget(saved_images_content_);
        il->addWidget(saved_images_scroll_, 1);
    }
    central_stack_->addWidget(saved_images_page_);

    central_stack_->setCurrentWidget(process_page_);
}

void MainWindow::setupStyleSheet() {
    setStyleSheet(QStringLiteral(
        "QMainWindow { background-color: #f5f5f5; }"
        "QWidget { color: #333333; font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif; font-size: 13px; }"
        "QDialog { background-color: #ffffff; }"
        "QMessageBox { background-color: #ffffff; }"
        "QMessageBox QLabel { color: #333333; font-size: 13px; background: transparent; }"
        "QMessageBox QPushButton { min-width: 80px; min-height: 28px; }"
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

    {
        QImage qimg = matToQImage(ref_img);
        QPixmap pixmap = QPixmap::fromImage(qimg);
        if (!pixmap.isNull()) {
            pixmap = pixmap.scaled(ref_original_thumbnail_->size(), Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
            ref_original_thumbnail_->setPixmap(pixmap);
        }
    }

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
    central_stack_->setCurrentWidget(process_page_);
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
    central_stack_->setCurrentWidget(process_page_);
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

    video_preview_->pause();  // 暂停预览播放器，后续由管线接管解码

    recognition_only_ = false;
    config_.behavior.skip_analysis = false;  // 完整模式：检测+识别+OpenFace分析
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

    // 重置所有运行状态和计数器
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
    run_start_time_ = last_fps_time_;  // 记录运行起始时刻，用于计算已运行时长

    DashboardData dd;
    fps_panel_->updateData(dd);
    counter_summary_->setText(QStringLiteral("<span style='color:#999999'>\u5c1a\u672a\u5f00\u59cb\u5904\u7406</span>"));

    setUiEnabled(true);
    status_label_->setText(QStringLiteral("\u6b63\u5728\u5904\u7406..."));
    completion_panel_->hide();

    // 延迟到事件循环空闲时启动，让 UI 先完成渲染
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

    if (pipeline_) last_output_dir_ = pipeline_->GetOutputDir();
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
    // 根据上次运行模式重新启动：保持仅识别或完整分析模式
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
    config_.behavior.skip_analysis = true;  // 仅识别模式：跳过OpenFace分析以提升速度
    config_.model.reference_image = reference_path_;
    config_.video.input_path = video_path_;

    try {
        pipeline_ = std::make_unique<Pipeline>(config_);  // 管线在构造时读取skip_analysis
        pipeline_->OpenVideo(video_path_);
    } catch (const std::exception& e) {
        QMessageBox::critical(this,
                              QStringLiteral("\u9519\u8bef"),
                              QStringLiteral("\u521d\u5316\u7ba1\u7ebf\u5931\u8d25: %1").arg(e.what()));
        config_.behavior.skip_analysis = false;  // 构造失败时恢复原值
        return;
    }
    config_.behavior.skip_analysis = false;  // 管线已创建，恢复配置原值以免影响后续操作

    video_fps_ = pipeline_->GetFps();
    total_frame_count_ = pipeline_->GetFrameCount();

    if (total_frame_count_ > 0) {
        processing_progress_->setRange(0, total_frame_count_);
        processing_progress_->setValue(0);
    }

    // 重置所有运行状态和计数器
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
    completion_panel_->hide();

    QTimer::singleShot(0, this, &MainWindow::processNextFrame);
}

void MainWindow::processNextFrame() {
    if (!is_running_) return;

    // 暂停状态下，仅单步请求才能继续
    if (is_paused_ && !step_requested_) return;

    cv::Mat frame;
    if (!pipeline_->PollFrame(frame)) {
        if (pipeline_->IsDecodeFinished()) {
            // 解码完毕，结束本次运行
            is_running_ = false;
            is_paused_ = false;
            has_finished_once_ = true;
            last_output_dir_ = pipeline_->GetOutputDir();
            pipeline_.reset();
            setFinishedState();
            status_label_->setText(QStringLiteral("\u5904\u7406\u5b8c\u6210"));
            if (total_frame_count_ > 0) {
                processing_progress_->setValue(total_frame_count_);
            }
            return;
        }
        // 解码尚未完成，稍后重试
        QTimer::singleShot(2, this, &MainWindow::processNextFrame);
        return;
    }

    frame_count_++;
    fps_frame_count_++;

    // 每秒统计一次瞬时帧率
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

    // 每0.5秒刷新一次仪表板和计数器摘要，避免过于频繁的UI更新
    double dash_elapsed = std::chrono::duration<double>(now - last_dashboard_time_).count();
    if (dash_elapsed >= 0.5) {
        last_dashboard_time_ = now;

        auto run_elapsed = std::chrono::duration<double>(now - run_start_time_).count();
        // 估算各类数据点数：每帧OpenFace输出17个AU、68×2个关键点、6个姿态值、2个视线值
        int64_t au_pts = total_openface_analyses_ * 17;
        int64_t lm_pts = total_openface_analyses_ * 68 * 2;
        int64_t pose_recs = total_openface_analyses_ * 6;
        int64_t gaze_recs = total_openface_analyses_ * 2;
        // 粗略估算数据量：OpenFace每帧约644字节 + 识别特征向量约2048字节
        double vol_mb = (total_openface_analyses_ * 644.0 + total_matches_ * 2048.0) / (1024.0 * 1024.0);

        // 从延迟换算子模块吞吐率：fps = 1000/毫秒
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

    // 单步模式：仅处理一帧后回到暂停状态，不自动调度下一帧
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

    if (!recognition_only_ && has_finished_once_) {
        completion_panel_->show();
    }
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
    central_stack_->setCurrentWidget(process_page_);

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

    central_stack_->setCurrentWidget(process_page_);
    spdlog::info("Selected saved image: {}", reference_path_);
}

void MainWindow::onViewDetails() {
    if (!last_output_dir_.empty()) {
        detail_from_history_ = false;
        showDetailPage(last_output_dir_);
    }
}

void MainWindow::onTaskHistory() {
    showHistoryPage();
}

void MainWindow::onBackToProcess() {
    if (central_stack_->currentWidget() == detail_page_) {
        detail_video_->stop();
        detail_orig_video_->stop();
        disconnect(detail_video_, &VideoPlayerWidget::positionChanged,
                   this, &MainWindow::onDetailFrameSync);

        detail_csv_headers_.clear();
        detail_csv_rows_.clear();
        detail_frame_to_row_.clear();
        detail_aligned_paths_.clear();
        detail_tracked_fps_ = 0.0;
        detail_face_thumb_->setText(QStringLiteral("\u5bf9\u9f50\u4eba\u8138"));
        detail_data_label_->setText(
            QStringLiteral("<span style='color:#999'>\u64ad\u653e\u89c6\u9891\u4ee5\u67e5\u770b\u6bcf\u5e27\u6570\u636e</span>"));

        fps_panel_->show();
        if (detail_from_history_) {
            central_stack_->setCurrentWidget(history_page_);
            return;
        }
    }
    central_stack_->setCurrentWidget(process_page_);
}

void MainWindow::onDeleteTask(const std::string& dir_path) {
    if (dir_path.empty()) return;
    namespace fs = std::filesystem;
    fs::path out_dir(dir_path);
    if (!fs::is_directory(out_dir)) return;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("\u786e\u8ba4\u5220\u9664"));
    msgBox.setText(QStringLiteral("\u786e\u5b9a\u8981\u5220\u9664\u8be5\u4efb\u52a1\u53ca\u5176\u6240\u6709\u6587\u4ef6\u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setStyleSheet(
        "QMessageBox { background-color: #ffffff; }"
        "QLabel { color: #333333; font-size: 13px; background: transparent; border: none; }"
        "QPushButton { min-width: 80px; min-height: 28px; background-color: #ffffff; "
        "border: 1px solid #c0c0c0; border-radius: 4px; padding: 6px 14px; color: #333333; }"
        "QPushButton:hover { background-color: #e8e8e8; }");
    if (msgBox.exec() != QMessageBox::Yes) return;

    // 先释放所有文件句柄（视频播放器可能持有输出目录中的文件）
    detail_video_->stop();
    detail_orig_video_->stop();

    std::error_code ec;
    fs::remove_all(out_dir, ec);
    if (ec) {
        spdlog::warn("Failed to delete task directory: {}", ec.message());
        QMessageBox::warning(this, QStringLiteral("\u5220\u9664\u5931\u8d25"),
                             QStringLiteral("\u65e0\u6cd5\u5220\u9664\u4efb\u52a1\u76ee\u5f55\uff0c\u53ef\u80fd\u6587\u4ef6\u6b63\u5728\u88ab\u4f7f\u7528\u3002"));
        return;
    }

    if (central_stack_->currentWidget() == detail_page_) {
        current_detail_dir_.clear();
        detail_csv_headers_.clear();
        detail_csv_rows_.clear();
        detail_frame_to_row_.clear();
        detail_aligned_paths_.clear();
        central_stack_->setCurrentWidget(process_page_);
    } else {
        showHistoryPage();
    }
}

void MainWindow::showDetailPage(const std::string& output_dir) {
    namespace fs = std::filesystem;
    fs::path out_dir(output_dir);
    if (!fs::is_directory(out_dir)) return;

    current_detail_dir_ = output_dir;

    detail_csv_headers_.clear();
    detail_csv_rows_.clear();
    detail_frame_to_row_.clear();
    detail_aligned_paths_.clear();
    detail_tracked_fps_ = 0.0;

    std::string csv_path;
    for (const auto& entry : fs::directory_iterator(out_dir)) {
        if (entry.path().extension() == ".csv") { csv_path = entry.path().string(); break; }
    }
    if (!csv_path.empty()) {
        std::ifstream csv_file(csv_path);
        if (csv_file.is_open()) {
            std::string line;
            auto parseRow = [](const std::string& line) {
                std::vector<std::string> cols;
                std::string cell;
                bool in_quotes = false;
                for (size_t i = 0; i < line.size(); ++i) {
                    char ch = line[i];
                    if (ch == '"') { in_quotes = !in_quotes; }
                    else if (ch == ',' && !in_quotes) { cols.push_back(cell); cell.clear(); }
                    else { cell += ch; }
                }
                cols.push_back(cell);
                return cols;
            };
            bool first = true;
            while (std::getline(csv_file, line)) {
                if (line.empty()) continue;
                if (first) { detail_csv_headers_ = parseRow(line); first = false; }
                else { detail_csv_rows_.push_back(parseRow(line)); }
            }
            int frame_col = -1;
            for (size_t i = 0; i < detail_csv_headers_.size(); ++i) {
                if (detail_csv_headers_[i] == "frame") { frame_col = static_cast<int>(i); break; }
            }
            if (frame_col >= 0) {
                for (size_t r = 0; r < detail_csv_rows_.size(); ++r) {
                    try {
                        int fnum = std::stoi(detail_csv_rows_[r][frame_col]);
                        detail_frame_to_row_[fnum] = static_cast<int>(r);
                    } catch (...) {}
                }
            }
        }
    }

    for (const auto& entry : fs::directory_iterator(out_dir)) {
        if (entry.is_directory() && entry.path().filename().string().find("_aligned") != std::string::npos) {
            for (const auto& f : fs::directory_iterator(entry.path())) {
                auto ext = f.path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".bmp")
                    detail_aligned_paths_.push_back(f.path().string());
            }
            std::sort(detail_aligned_paths_.begin(), detail_aligned_paths_.end());
            break;
        }
    }

    std::string tracked_path;
    for (const auto& entry : fs::directory_iterator(out_dir)) {
        if (entry.path().extension() == ".avi") { tracked_path = entry.path().string(); break; }
    }
    int video_w = 0, video_h = 0;
    if (!tracked_path.empty()) {
        cv::VideoCapture cap(tracked_path);
        if (cap.isOpened()) {
            detail_tracked_fps_ = cap.get(cv::CAP_PROP_FPS);
            video_w = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
            video_h = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
            cap.release();
        }
        detail_video_->setVideoPath(tracked_path);
    }

    std::string dirname = out_dir.filename().string();
    std::string video_name;
    std::string time_display;
    if (dirname.size() >= 16) {
        size_t last_underscore = dirname.rfind('_');
        size_t date_underscore = (last_underscore != std::string::npos)
            ? dirname.rfind('_', last_underscore - 1) : std::string::npos;
        if (date_underscore != std::string::npos && last_underscore != std::string::npos) {
            video_name = dirname.substr(0, date_underscore);
            std::string date_part = dirname.substr(date_underscore + 1, 8);
            std::string time_part = dirname.substr(last_underscore + 1, 6);
            time_display = date_part.substr(0, 4) + "-" + date_part.substr(4, 2) + "-"
                         + date_part.substr(6, 2) + " "
                         + time_part.substr(0, 2) + ":" + time_part.substr(2, 2) + ":"
                         + time_part.substr(4, 2);
        }
    }
    if (video_name.empty()) video_name = dirname;

    std::string orig_video_path;
    for (const auto& entry : fs::directory_iterator(out_dir)) {
        auto fname = entry.path().filename().string();
        if (fname.find("original") == 0 && entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov") {
                orig_video_path = entry.path().string();
                break;
            }
        }
    }
    if (orig_video_path.empty()) orig_video_path = tracked_path;
    detail_orig_video_->setVideoPath(orig_video_path);

    detail_ref_thumb_->setText(QStringLiteral("\u76ee\u6807\u4eba\u7269"));
    detail_ref_detected_->setText(QStringLiteral("\u68c0\u6d4b\u4eba\u8138"));
    for (const auto& entry : fs::directory_iterator(out_dir)) {
        auto fname = entry.path().filename().string();
        if (!entry.is_regular_file()) continue;
        if (fname.find("reference") != 0) continue;

        cv::Mat img = cv::imread(entry.path().string());
        if (img.empty()) continue;
        QImage qimg = matToQImage(img);
        QPixmap pixmap = QPixmap::fromImage(qimg);
        if (pixmap.isNull()) continue;

        QPixmap scaled = pixmap.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (fname.find("_detected") != std::string::npos) {
            detail_ref_detected_->setPixmap(scaled);
        } else {
            detail_ref_thumb_->setPixmap(scaled);
        }
    }

    int64_t file_size = 0;
    int total_frames = 0;
    if (!orig_video_path.empty()) {
        try { file_size = fs::file_size(orig_video_path); } catch (...) {}
        cv::VideoCapture cap(orig_video_path);
        if (cap.isOpened()) {
            total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
            cap.release();
        }
    }

    auto fmtSize = [](int64_t bytes) -> QString {
        if (bytes >= 1024 * 1024 * 1024)
            return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
        if (bytes >= 1024 * 1024)
            return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
        if (bytes >= 1024)
            return QString::number(bytes / 1024.0, 'f', 1) + " KB";
        return QString::number(bytes) + " B";
    };

    QString info_html;
    info_html += "<div style='font-size:12px; line-height:1.6; color:#555;'>";
    info_html += QStringLiteral(
        "<table>"
        "<tr><td style='color:#999; padding-right:8px;'>\u89c6\u9891\u540d\u79f0</td>"
        "<td><b>%1</b></td></tr>"
        "<tr><td style='color:#999; padding-right:8px;'>\u89c6\u9891\u5927\u5c0f</td>"
        "<td>%2</td></tr>"
        "<tr><td style='color:#999; padding-right:8px;'>\u5904\u7406\u65f6\u95f4</td>"
        "<td>%3</td></tr>"
        "<tr><td style='color:#999; padding-right:8px;'>\u5206\u8fa8\u7387</td>"
        "<td>%4\u00d7%5</td></tr>"
        "<tr><td style='color:#999; padding-right:8px;'>\u603b\u5e27\u6570</td>"
        "<td>%6</td></tr>"
        "</table>")
        .arg(QString::fromStdString(video_name))
        .arg(fmtSize(file_size))
        .arg(QString::fromStdString(time_display))
        .arg(video_w).arg(video_h)
        .arg(total_frames);
    info_html += "</div>";
    detail_info_label_->setText(info_html);

    disconnect(detail_video_, &VideoPlayerWidget::positionChanged,
               this, &MainWindow::onDetailFrameSync);
    connect(detail_video_, &VideoPlayerWidget::positionChanged,
            this, &MainWindow::onDetailFrameSync);

    fps_panel_->hide();
    central_stack_->setCurrentWidget(detail_page_);
    if (!tracked_path.empty()) detail_video_->play();
}

void MainWindow::showHistoryPage() {
    namespace fs = std::filesystem;
    std::string app_dir = QCoreApplication::applicationDirPath().toStdString();
    fs::path output_dir = fs::path(app_dir) / "data" / "output";

    if (history_content_->layout()) {
        QLayoutItem* item;
        while ((item = history_content_->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    auto* hc_layout = static_cast<QVBoxLayout*>(history_content_->layout());

    if (!fs::is_directory(output_dir)) {
        auto* empty_label = new QLabel(QStringLiteral("\u6682\u65e0\u5904\u7406\u8bb0\u5f55"));
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("color: #999; font-size: 14px; padding: 60px 0;");
        hc_layout->addWidget(empty_label);
        hc_layout->addStretch();
        central_stack_->setCurrentWidget(history_page_);
        return;
    }

    struct TaskInfo {
        std::string dir_path, video_name, time_str;
        int frame_count = 0;
    };
    std::vector<TaskInfo> tasks;

    for (const auto& entry : fs::directory_iterator(output_dir)) {
        if (!entry.is_directory()) continue;
        std::string dirname = entry.path().filename().string();
        if (dirname.size() < 16) continue;
        size_t last_underscore = dirname.rfind('_');
        if (last_underscore == std::string::npos || last_underscore < 9) continue;
        size_t date_underscore = dirname.rfind('_', last_underscore - 1);
        if (date_underscore == std::string::npos || date_underscore < 1) continue;

        std::string date_part = dirname.substr(date_underscore + 1, 8);
        std::string time_part = dirname.substr(last_underscore + 1, 6);

        TaskInfo info;
        info.dir_path = entry.path().string();
        info.video_name = dirname.substr(0, date_underscore);
        info.time_str = date_part.substr(0, 4) + "-" + date_part.substr(4, 2) + "-"
                        + date_part.substr(6, 2) + " "
                        + time_part.substr(0, 2) + ":" + time_part.substr(2, 2) + ":"
                        + time_part.substr(4, 2);

        for (const auto& f : fs::directory_iterator(entry.path())) {
            if (f.path().extension() == ".csv") {
                std::ifstream csv_file(f.path().string());
                if (csv_file.is_open()) {
                    std::string line;
                    while (std::getline(csv_file, line))
                        if (!line.empty()) ++info.frame_count;
                    if (info.frame_count > 0) --info.frame_count;
                }
            }
        }
        tasks.push_back(std::move(info));
    }

    if (tasks.empty()) {
        auto* empty_label = new QLabel(QStringLiteral("\u6682\u65e0\u5904\u7406\u8bb0\u5f55"));
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("color: #999; font-size: 14px; padding: 60px 0;");
        hc_layout->addWidget(empty_label);
        hc_layout->addStretch();
        central_stack_->setCurrentWidget(history_page_);
        return;
    }

    std::sort(tasks.begin(), tasks.end(),
              [](const TaskInfo& a, const TaskInfo& b) { return a.time_str > b.time_str; });

    for (const auto& t : tasks) {
        auto* card = new QFrame;
        card->setStyleSheet(
            "QFrame { background-color: #ffffff; border: 1px solid #e0e0e0; "
            "border-radius: 6px; }"
            "QFrame:hover { border-color: #90caf9; background-color: #fafcff; }");
        auto* card_layout = new QHBoxLayout(card);
        card_layout->setContentsMargins(16, 12, 16, 12);
        card_layout->setSpacing(12);

        auto* info_widget = new QWidget;
        auto* info_layout = new QVBoxLayout(info_widget);
        info_layout->setContentsMargins(0, 0, 0, 0);
        info_layout->setSpacing(2);

        auto* name_label = new QLabel(QString::fromStdString(t.video_name));
        name_label->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; border: none;");
        info_layout->addWidget(name_label);

        auto* meta_label = new QLabel(
            QStringLiteral("%1  |  %2 \u5e27")
                .arg(QString::fromStdString(t.time_str))
                .arg(t.frame_count));
        meta_label->setStyleSheet("font-size: 12px; color: #999; border: none;");
        info_layout->addWidget(meta_label);

        card_layout->addWidget(info_widget, 1);

        auto* view_btn = new QPushButton(QStringLiteral("\u67e5\u770b\u8be6\u60c5"));
        view_btn->setFixedHeight(32);
        view_btn->setStyleSheet(
            "QPushButton { background-color: #1565c0; color: white; border: none; "
            "border-radius: 4px; padding: 6px 18px; font-size: 12px; font-weight: bold; }"
            "QPushButton:hover { background-color: #1976d2; }");
        std::string dir_path = t.dir_path;
        connect(view_btn, &QPushButton::clicked, this, [this, dir_path]() {
            detail_from_history_ = true;
            showDetailPage(dir_path);
        });
        card_layout->addWidget(view_btn);

        auto* del_btn = new QPushButton(QStringLiteral("\u5220\u9664"));
        del_btn->setFixedHeight(32);
        del_btn->setStyleSheet(
            "QPushButton { background-color: #e53935; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #c62828; }");
        connect(del_btn, &QPushButton::clicked, this, [this, dir_path]() {
            onDeleteTask(dir_path);
        });
        card_layout->addWidget(del_btn);

        hc_layout->addWidget(card);
    }

    hc_layout->addStretch();
    central_stack_->setCurrentWidget(history_page_);
}

void MainWindow::showSavedVideosPage() {
    namespace fs = std::filesystem;
    std::string app_dir = QCoreApplication::applicationDirPath().toStdString();
    fs::path upload_dir = fs::path(app_dir) / "data" / "upload" / "video";

    if (saved_videos_content_->layout()) {
        QLayoutItem* item;
        while ((item = saved_videos_content_->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }
    auto* vc_layout = static_cast<QVBoxLayout*>(saved_videos_content_->layout());
    saved_video_preview_->stop();
    saved_video_preview_->hide();

    std::vector<std::string> videos;
    if (fs::is_directory(upload_dir)) {
        for (const auto& entry : fs::directory_iterator(upload_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov")
                videos.push_back(entry.path().string());
        }
    }

    if (videos.empty()) {
        auto* empty_label = new QLabel(QStringLiteral("\u6682\u65e0\u4e0a\u4f20\u7684\u89c6\u9891"));
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("color: #999; font-size: 14px; padding: 60px 0;");
        vc_layout->addWidget(empty_label);
        vc_layout->addStretch();
        central_stack_->setCurrentWidget(saved_videos_page_);
        return;
    }

    std::sort(videos.begin(), videos.end());
    for (const auto& vpath : videos) {
        fs::path p(vpath);
        std::string fname = p.filename().string();

        auto* card = new QFrame;
        card->setStyleSheet(
            "QFrame { background-color: #ffffff; border: 1px solid #e0e0e0; "
            "border-radius: 6px; }"
            "QFrame:hover { border-color: #90caf9; }");
        auto* card_layout = new QHBoxLayout(card);
        card_layout->setContentsMargins(16, 12, 16, 12);
        card_layout->setSpacing(12);

        auto* name_label = new QLabel(QString::fromStdString(fname));
        name_label->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; border: none;");
        card_layout->addWidget(name_label, 1);

        std::string path_copy = vpath;
        auto* play_btn = new QPushButton(QStringLiteral("\u64ad\u653e"));
        play_btn->setFixedHeight(32);
        play_btn->setStyleSheet(
            "QPushButton { background-color: #43a047; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #388e3c; }");
        connect(play_btn, &QPushButton::clicked, this, [this, path_copy]() {
            saved_video_preview_->setVideoPath(path_copy);
            saved_video_preview_->show();
            saved_video_preview_->play();
        });
        card_layout->addWidget(play_btn);

        auto* del_btn = new QPushButton(QStringLiteral("\u5220\u9664"));
        del_btn->setFixedHeight(32);
        del_btn->setStyleSheet(
            "QPushButton { background-color: #e53935; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #c62828; }");
        connect(del_btn, &QPushButton::clicked, this, [this, path_copy]() {
            onDeleteSavedFile(path_copy);
        });
        card_layout->addWidget(del_btn);

        vc_layout->addWidget(card);
    }

    vc_layout->addStretch();
    central_stack_->setCurrentWidget(saved_videos_page_);
}

void MainWindow::showSavedImagesPage() {
    namespace fs = std::filesystem;
    std::string app_dir = QCoreApplication::applicationDirPath().toStdString();
    fs::path upload_dir = fs::path(app_dir) / "data" / "upload" / "img";

    if (saved_images_content_->layout()) {
        QLayoutItem* item;
        while ((item = saved_images_content_->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }
    auto* ic_layout = static_cast<QVBoxLayout*>(saved_images_content_->layout());

    std::vector<std::string> images;
    if (fs::is_directory(upload_dir)) {
        for (const auto& entry : fs::directory_iterator(upload_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp")
                images.push_back(entry.path().string());
        }
    }

    if (images.empty()) {
        auto* empty_label = new QLabel(QStringLiteral("\u6682\u65e0\u4e0a\u4f20\u7684\u56fe\u7247"));
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("color: #999; font-size: 14px; padding: 60px 0;");
        ic_layout->addWidget(empty_label);
        ic_layout->addStretch();
        central_stack_->setCurrentWidget(saved_images_page_);
        return;
    }

    std::sort(images.begin(), images.end());
    for (const auto& img_path : images) {
        fs::path p(img_path);
        std::string fname = p.filename().string();

        auto* card = new QFrame;
        card->setStyleSheet(
            "QFrame { background-color: #ffffff; border: 1px solid #e0e0e0; "
            "border-radius: 6px; }"
            "QFrame:hover { border-color: #90caf9; }");
        auto* card_layout = new QHBoxLayout(card);
        card_layout->setContentsMargins(16, 12, 16, 12);
        card_layout->setSpacing(12);

        auto* thumb = new QLabel;
        thumb->setFixedSize(80, 80);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setStyleSheet(
            "background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px;");
        cv::Mat img_mat = cv::imread(img_path);
        if (!img_mat.empty()) {
            QImage qimg = matToQImage(img_mat);
            QPixmap pm = QPixmap::fromImage(qimg);
            if (!pm.isNull())
                thumb->setPixmap(pm.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            thumb->setText(QStringLiteral("\u56fe\u7247"));
        }
        card_layout->addWidget(thumb);

        auto* name_label = new QLabel(QString::fromStdString(fname));
        name_label->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; border: none;");
        card_layout->addWidget(name_label, 1);

        std::string path_copy = img_path;
        auto* view_btn = new QPushButton(QStringLiteral("\u67e5\u770b\u5927\u56fe"));
        view_btn->setFixedHeight(32);
        view_btn->setStyleSheet(
            "QPushButton { background-color: #1565c0; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #1976d2; }");
        connect(view_btn, &QPushButton::clicked, this, [this, path_copy]() {
            cv::Mat m = cv::imread(path_copy);
            if (!m.empty()) {
                QImage qimg = matToQImage(m);
                QDialog* dlg = new QDialog(this);
                dlg->setWindowTitle(QStringLiteral("\u56fe\u7247\u9884\u89c8"));
                dlg->setStyleSheet("background-color: #ffffff;");
                dlg->setMinimumSize(400, 300);
                auto* dlg_layout = new QVBoxLayout(dlg);
                auto* img_label = new QLabel;
                img_label->setPixmap(QPixmap::fromImage(qimg).scaled(
                    600, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                img_label->setAlignment(Qt::AlignCenter);
                dlg_layout->addWidget(img_label);
                dlg->exec();
                dlg->deleteLater();
            }
        });
        card_layout->addWidget(view_btn);

        auto* del_btn = new QPushButton(QStringLiteral("\u5220\u9664"));
        del_btn->setFixedHeight(32);
        del_btn->setStyleSheet(
            "QPushButton { background-color: #e53935; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #c62828; }");
        connect(del_btn, &QPushButton::clicked, this, [this, path_copy]() {
            onDeleteSavedFile(path_copy);
        });
        card_layout->addWidget(del_btn);

        ic_layout->addWidget(card);
    }

    ic_layout->addStretch();
    central_stack_->setCurrentWidget(saved_images_page_);
}

void MainWindow::onDeleteSavedFile(const std::string& file_path) {
    namespace fs = std::filesystem;
    if (!fs::is_regular_file(file_path)) return;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("\u786e\u8ba4\u5220\u9664"));
    msgBox.setText(QStringLiteral("\u786e\u5b9a\u8981\u5220\u9664\u8be5\u6587\u4ef6\u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setStyleSheet(
        "QMessageBox { background-color: #ffffff; }"
        "QLabel { color: #333333; font-size: 13px; background: transparent; border: none; }"
        "QPushButton { min-width: 80px; min-height: 28px; background-color: #ffffff; "
        "border: 1px solid #c0c0c0; border-radius: 4px; padding: 6px 14px; color: #333333; }"
        "QPushButton:hover { background-color: #e8e8e8; }");
    if (msgBox.exec() != QMessageBox::Yes) return;

    // 停止预览（如果是视频）
    saved_video_preview_->stop();
    saved_video_preview_->hide();

    std::error_code ec;
    fs::remove(file_path, ec);
    if (ec) {
        spdlog::warn("Failed to delete file: {}", ec.message());
        return;
    }

    // 刷新当前页面
    if (central_stack_->currentWidget() == saved_videos_page_)
        showSavedVideosPage();
    else if (central_stack_->currentWidget() == saved_images_page_)
        showSavedImagesPage();
}

void MainWindow::onDetailFrameSync(qint64 positionMs) {
    if (detail_csv_rows_.empty() || detail_frame_to_row_.empty() || detail_tracked_fps_ <= 0.0) return;

    // 将播放位置（毫秒）转换为视频帧号，再映射到CSV中的帧号（CSV帧号从1开始）
    int video_frame = static_cast<int>(positionMs * detail_tracked_fps_ / 1000.0 + 0.5);
    int csv_frame_num = video_frame + 1;

    // 模糊查找：精确匹配失败时尝试前后一帧，应对帧率不完全对齐的情况
    auto it = detail_frame_to_row_.find(csv_frame_num);
    if (it == detail_frame_to_row_.end()) {
        it = detail_frame_to_row_.find(csv_frame_num - 1);
        if (it == detail_frame_to_row_.end())
            it = detail_frame_to_row_.find(csv_frame_num + 1);
    }
    if (it == detail_frame_to_row_.end()) return;

    int row_idx = it->second;
    const auto& row = detail_csv_rows_[row_idx];
    if (row.empty() || detail_csv_headers_.empty()) return;

    // 辅助lambda：按列名查找列索引、按索引取字符串值/浮点值
    auto col = [this](const std::string& name) -> int {
        for (size_t i = 0; i < detail_csv_headers_.size(); ++i)
            if (detail_csv_headers_[i] == name) return static_cast<int>(i);
        return -1;
    };
    auto val = [&row](int idx) -> std::string {
        if (idx < 0 || idx >= static_cast<int>(row.size())) return "0";
        return row[idx];
    };
    auto valf = [&val](int idx) -> double {
        try { return std::stod(val(idx)); } catch (...) { return 0.0; }
    };

    double pitch = valf(col("pose_Rx"));
    double yaw   = valf(col("pose_Ry"));
    double roll  = valf(col("pose_Rz"));
    double gaze_x = valf(col("gaze_angle_x"));
    double gaze_y = valf(col("gaze_angle_y"));
    int success = static_cast<int>(valf(col("success")));

    double face_min_x = 1e9, face_max_x = -1e9;
    double face_min_y = 1e9, face_max_y = -1e9;
    int lm_found = 0;
    for (int i = 0; i < 68; ++i) {
        int cx = col("x_" + std::to_string(i));
        int cy = col("y_" + std::to_string(i));
        if (cx < 0 || cy < 0) continue;
        double px = valf(cx), py = valf(cy);
        if (px == 0.0 && py == 0.0) continue;
        ++lm_found;
        if (px < face_min_x) face_min_x = px;
        if (px > face_max_x) face_max_x = px;
        if (py < face_min_y) face_min_y = py;
        if (py > face_max_y) face_max_y = py;
    }
    // 质量评分：正面度（pose偏转角越小越好，35°时降为0）权重0.6 + 人脸尺寸（40000像素时满分）权重0.4
    double pose_dev = std::sqrt(pitch * pitch + yaw * yaw);
    double frontal_score = std::max(0.0, 1.0 - pose_dev / 35.0);
    double face_area = (face_max_x - face_min_x) * (face_max_y - face_min_y);
    double size_score = (lm_found >= 10) ? std::min(1.0, face_area / 40000.0) : 0.0;
    double quality_val = frontal_score * 0.6 + size_score * 0.4;

    QString quality;
    if (success == 0 || lm_found < 10)
        quality = QStringLiteral("<span style='color:#e53935;'>\u68c0\u6d4b\u5931\u8d25</span>");
    else if (quality_val > 0.80)
        quality = QStringLiteral("<span style='color:#2e7d32;'>\u4f18\u79c0</span>");
    else if (quality_val > 0.55)
        quality = QStringLiteral("<span style='color:#4caf50;'>\u826f\u597d</span>");
    else if (quality_val > 0.30)
        quality = QStringLiteral("<span style='color:#f9a825;'>\u4e00\u822c</span>");
    else
        quality = QStringLiteral("<span style='color:#e53935;'>\u8f83\u5dee</span>");

    // 将欧拉角映射为自然语言描述（pitch=抬头，yaw=水平转头，roll=侧头倾斜）
    QStringList pose_parts;
    if (pitch > 15.0) pose_parts << QStringLiteral("\u660e\u663e\u62ac\u5934");
    else if (pitch > 5.0) pose_parts << QStringLiteral("\u7565\u5fae\u62ac\u5934");
    else if (pitch < -15.0) pose_parts << QStringLiteral("\u660e\u663e\u4f4e\u5934");
    else if (pitch < -5.0) pose_parts << QStringLiteral("\u7565\u5fae\u4f4e\u5934");
    if (yaw > 15.0) pose_parts << QStringLiteral("\u660e\u663e\u53f3\u8f6c");
    else if (yaw > 5.0) pose_parts << QStringLiteral("\u5fae\u5fae\u53f3\u8f6c");
    else if (yaw < -15.0) pose_parts << QStringLiteral("\u660e\u663e\u5de6\u8f6c");
    else if (yaw < -5.0) pose_parts << QStringLiteral("\u5fae\u5fae\u5de6\u8f6c");
    if (roll > 10.0) pose_parts << QStringLiteral("\u5934\u90e8\u53f3\u503e");
    else if (roll < -10.0) pose_parts << QStringLiteral("\u5934\u90e8\u5de6\u503e");
    QString pose_desc = pose_parts.isEmpty()
        ? QStringLiteral("\u6b63\u5bf9\u955c\u5934")
        : pose_parts.join(QStringLiteral("\u3001"));

    QString h_dir, v_dir;
    if (gaze_x > 0.3) h_dir = QStringLiteral("\u5411\u53f3");
    else if (gaze_x < -0.3) h_dir = QStringLiteral("\u5411\u5de6");
    if (gaze_y > 0.3) v_dir = QStringLiteral("\u5411\u4e0a");
    else if (gaze_y < -0.3) v_dir = QStringLiteral("\u5411\u4e0b");
    QString gaze_desc;
    if (h_dir.isEmpty() && v_dir.isEmpty())
        gaze_desc = QStringLiteral("\u6b63\u89c6\u524d\u65b9");
    else if (h_dir.isEmpty())
        gaze_desc = QStringLiteral("\u770b%1\u65b9").arg(v_dir);
    else if (v_dir.isEmpty())
        gaze_desc = QStringLiteral("\u770b%1\u65b9").arg(h_dir);
    else
        gaze_desc = QStringLiteral("\u770b%1%2\u65b9").arg(v_dir, h_dir);

    static const std::unordered_map<std::string, QString> kAuNames = {
        {"AU01", QStringLiteral("\u7709\u6bdb\u4e0a\u626c")},
        {"AU02", QStringLiteral("\u7709\u6bdb\u5916\u4fa7\u4e0a\u626c")},
        {"AU04", QStringLiteral("\u76b1\u7709")},
        {"AU05", QStringLiteral("\u7741\u5927\u773c")},
        {"AU06", QStringLiteral("\u8138\u988a\u63d0\u5347")},
        {"AU07", QStringLiteral("\u772f\u773c")},
        {"AU09", QStringLiteral("\u76b1\u9f3b")},
        {"AU10", QStringLiteral("\u4e0a\u5507\u4e0a\u63d0")},
        {"AU12", QStringLiteral("\u5634\u89d2\u4e0a\u626c")},
        {"AU14", QStringLiteral("\u62bf\u5634")},
        {"AU15", QStringLiteral("\u5634\u89d2\u4e0b\u6487")},
        {"AU17", QStringLiteral("\u4e0b\u5df4\u6536\u7d27")},
        {"AU20", QStringLiteral("\u5634\u89d2\u62c9\u4f38")},
        {"AU23", QStringLiteral("\u5634\u5507\u6536\u7d27")},
        {"AU25", QStringLiteral("\u5f20\u5634")},
        {"AU26", QStringLiteral("\u4e0b\u5df4\u4e0b\u5760")},
        {"AU28", QStringLiteral("\u5438\u5507")},
        {"AU45", QStringLiteral("\u7728\u773c")},
    };

    // 筛选CSV中以"_r"结尾的AU回归值列（如AU01_r），强度>0.3视为激活
    QStringList au_parts;
    for (size_t h = 0; h < detail_csv_headers_.size(); ++h) {
        const auto& hdr = detail_csv_headers_[h];
        if (hdr.size() > 3 && hdr.substr(0, 2) == "AU" &&
            hdr[hdr.size() - 2] == '_' && hdr.back() == 'r') {
            double intensity = valf(static_cast<int>(h));
            if (intensity > 0.3) {
                std::string au_code = hdr.substr(0, hdr.size() - 2);
                auto au_it = kAuNames.find(au_code);
                if (au_it != kAuNames.end()) au_parts << au_it->second;
                if (au_parts.size() >= 6) break;
            }
        }
    }
    QString au_desc = au_parts.isEmpty()
        ? QStringLiteral("\u65e0\u660e\u663e\u8868\u60c5")
        : au_parts.join(QStringLiteral("\u3001"));

    QString html;
    html += "<div style='font-size:13px; line-height:1.8; color:#333;'>";
    html += QStringLiteral(
        "<div>"
        "<span style='color:#888;'>\u7b2c </span><b>%1</b>"
        "<span style='color:#888;'> \u5e27 &nbsp;|&nbsp; \u65f6\u95f4 </span><b>%2 \u79d2</b>"
        "<span style='color:#888;'> &nbsp;|&nbsp; \u8d28\u91cf </span><b>%3</b>"
        "</div>")
        .arg(QString::fromStdString(val(col("frame"))))
        .arg(QString::fromStdString(val(col("timestamp"))))
        .arg(quality);
    html += QStringLiteral(
        "<div style='margin-top:4px;'>"
        "<span style='color:#888;'>\u5934\u90e8\u59ff\u6001\uff1a</span><b>%1</b>"
        "</div>").arg(pose_desc);
    html += QStringLiteral(
        "<div>"
        "<span style='color:#888;'>\u89c6\u7ebf\u65b9\u5411\uff1a</span><b>%1</b>"
        "</div>").arg(gaze_desc);
    html += QStringLiteral(
        "<div>"
        "<span style='color:#888;'>\u9762\u90e8\u8868\u60c5\uff1a</span><b>%1</b>"
        "</div>").arg(au_desc);
    html += "</div>";
    detail_data_label_->setText(html);

    if (row_idx < static_cast<int>(detail_aligned_paths_.size())) {
        cv::Mat face = cv::imread(detail_aligned_paths_[row_idx]);
        if (!face.empty()) {
            cv::Mat rgb;
            cv::cvtColor(face, rgb, cv::COLOR_BGR2RGB);
            QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
            detail_face_thumb_->setPixmap(QPixmap::fromImage(qimg.copy())
                .scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }
    detail_face_thumb_->setText(QStringLiteral("\u5bf9\u9f50\u4eba\u8138"));
}

}  // namespace omniface::ui
