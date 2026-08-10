#include "omniface/ui/pages/processing_page.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <spdlog/spdlog.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/align.h"
#include "omniface/core/recognition/face_recognizer.h"
#include "omniface/db/image_repo.h"
#include "omniface/db/snowflake.h"
#include "omniface/db/video_repo.h"
#include "omniface/pipeline/framework/base_pipeline.h"
#include "omniface/pipeline/framework/pipeline_factory.h"
#include "omniface/ui/dialogs/image_selection_dialog.h"
#include "omniface/ui/dialogs/video_selection_dialog.h"
#include "omniface/ui/theme.h"
#include "omniface/ui/widgets/video_player_widget.h"

namespace omniface {
namespace ui {

namespace {

QString DataVideoDir() {
    QString dir = QApplication::applicationDirPath() + QStringLiteral("/data/videos");
    QDir().mkpath(dir);
    return dir;
}

QString DataImageDir() {
    QString dir = QApplication::applicationDirPath() + QStringLiteral("/data/images");
    QDir().mkpath(dir);
    return dir;
}

QString ResolveModelPath(const QString& relative) {
    QString appDir = QApplication::applicationDirPath();
    QString path = appDir + QStringLiteral("/") + relative;
    if (QFile::exists(path)) return QFileInfo(path).absoluteFilePath();
    path = appDir + QStringLiteral("/../") + relative;
    if (QFile::exists(path)) return QFileInfo(path).absoluteFilePath();
    path = appDir + QStringLiteral("/../../") + relative;
    if (QFile::exists(path)) return QFileInfo(path).absoluteFilePath();
    return relative;
}

QString SelectedBtnStyle() {
    auto& tm = ThemeManager::Instance();
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 12px; color: %3; }"
        "QPushButton:hover { border-color: %4; }"
        "QPushButton::menu-indicator { image: none; width: 0px; }")
        .arg(tm.SurfaceBg(), tm.Primary(), tm.TextPrimary(), tm.AccentBlue());
}

QString DefaultBtnStyle() {
    auto& tm = ThemeManager::Instance();
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 12px; color: %3; }"
        "QPushButton:hover { border-color: %4; }"
        "QPushButton::menu-indicator { image: none; width: 0px; }")
        .arg(tm.SurfaceBg(), tm.Border(), tm.TextMuted(), tm.Primary());
}

QString MenuStyle() {
    auto& tm = ThemeManager::Instance();
    return QStringLiteral(
        "QMenu { background: %1; color: %2; border: 1px solid %3; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; }"
        "QMenu::item:selected { background: %4; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.SelectedBg());
}

QString PanelStyle() {
    auto& tm = ThemeManager::Instance();
    return QStringLiteral(
        "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
        "  border-radius: 4px; margin-top: 10px; padding-top: 14px; background: %3; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }")
        .arg(tm.TextPrimary(), tm.Border(), tm.AltRow());
}

QString StatTitleStyle() {
    auto& tm = ThemeManager::Instance();
    return QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(tm.TextMuted());
}

QString StatValueStyle() {
    auto& tm = ThemeManager::Instance();
    return QStringLiteral("QLabel { color: %1; font-size: 11px; font-weight: bold; }").arg(tm.TextPrimary());
}

QString SectionHeaderStyle() {
    auto& tm = ThemeManager::Instance();
    return QStringLiteral(
        "QLabel { color: %1; font-size: 9px; font-weight: bold;"
        "  padding-top: 6px; padding-bottom: 2px; }").arg(tm.TextDim());
}

void ResolveConfigPaths(PipelineConfig& config) {
    config.model.detector_model = ResolveModelPath(
        QString::fromStdString(config.model.detector_model)).toStdString();
    config.model.recognizer_model = ResolveModelPath(
        QString::fromStdString(config.model.recognizer_model)).toStdString();
    config.model.openface_model_dir = ResolveModelPath(
        QString::fromStdString(config.model.openface_model_dir)).toStdString();
    config.l2cs.model = ResolveModelPath(
        QString::fromStdString(config.l2cs.model)).toStdString();
    config.mefarg_au.model = ResolveModelPath(
        QString::fromStdString(config.mefarg_au.model)).toStdString();
    config.onnx.landmark_model = ResolveModelPath(
        QString::fromStdString(config.onnx.landmark_model)).toStdString();
    config.onnx.canonical_model = ResolveModelPath(
        QString::fromStdString(config.onnx.canonical_model)).toStdString();
    if (!config.onnx.au_model.empty())
        config.onnx.au_model = ResolveModelPath(
            QString::fromStdString(config.onnx.au_model)).toStdString();
}

}  // namespace

// ═══════════════════════════════════════════════════════════════
// FrameDisplayWidget — high-performance frame display
// ═══════════════════════════════════════════════════════════════

FrameDisplayWidget::FrameDisplayWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(320, 240);
    auto& tm = ThemeManager::Instance();
    setStyleSheet(QStringLiteral("QWidget { background: %1; border: 1px solid %2; }")
        .arg(tm.DeepBg(), tm.Border()));
}

void FrameDisplayWidget::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();
    setStyleSheet(QStringLiteral("QWidget { background: %1; border: 1px solid %2; }")
        .arg(tm.DeepBg(), tm.Border()));
    update();
}

void FrameDisplayWidget::SetFrame(const cv::Mat& bgr) {
    if (bgr.empty()) return;

    QImage img(bgr.data, bgr.cols, bgr.rows,
               static_cast<int>(bgr.step), QImage::Format_RGB888);
    QImage rgb = img.rgbSwapped();  // Shallow — no copy, just swaps R/B interpretation

    QSize sz = size();
    if (sz.width() > 0 && sz.height() > 0) {
        QImage scaled = rgb.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        {
            QMutexLocker lock(&frame_mutex_);
            current_frame_ = QPixmap::fromImage(scaled);
        }
    }

    update();
}

void FrameDisplayWidget::paintEvent(QPaintEvent*) {
    auto& tm = ThemeManager::Instance();
    QPainter p(this);
    p.fillRect(rect(), QColor(tm.DeepBg()));

    QMutexLocker lock(&frame_mutex_);
    if (current_frame_.isNull()) {
        p.setPen(QColor(tm.TextDim()));
        p.setFont(QFont("Arial", 14));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("实时画面"));
        return;
    }

    int x = (width() - current_frame_.width()) / 2;
    int y = (height() - current_frame_.height()) / 2;
    p.drawPixmap(x, y, current_frame_);
}

// ═══════════════════════════════════════════════════════════════
// ProcessWorker
// ═══════════════════════════════════════════════════════════════

ProcessWorker::ProcessWorker(QObject* parent) : QObject(parent) {}

ProcessWorker::~ProcessWorker() {
    Stop();
    if (thread_ && thread_->joinable())
        thread_->join();
}

void ProcessWorker::SetVideoPath(const QString& path) { video_path_ = path; }
void ProcessWorker::SetReference(const FaceEmbedding& embedding) { reference_ = embedding; }
void ProcessWorker::SetConfig(const PipelineConfig& config) { config_ = config; }

void ProcessWorker::Start() {
    if (running_) return;

    try {
        pipeline_ = CreatePipeline(config_.analysis_backend, config_, reference_, nullptr);
        if (pipeline_)
            pipeline_->SetVideoSource(video_path_.toStdString());
    } catch (const std::exception& e) {
        emit ProcessingError(QStringLiteral("创建处理管线失败:\n%1").arg(QString::fromStdString(e.what())));
        return;
    }

    running_ = true;
    paused_ = false;

    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
    thread_ = std::make_unique<std::thread>(&ProcessWorker::ProcessLoop, this);
}

void ProcessWorker::Pause() { paused_ = true; }
void ProcessWorker::Resume() { paused_ = false; }
void ProcessWorker::Step() { step_requested_ = true; }

void ProcessWorker::Stop() {
    running_ = false;
    paused_ = false;
}

std::string ProcessWorker::GetOutputDir() const {
    return pipeline_ ? pipeline_->OutputDir() : "";
}

std::string ProcessWorker::GetOutputVideoPath() const {
    return pipeline_ ? pipeline_->OutputVideoPath() : "";
}

void ProcessWorker::ProcessLoop() {
    cv::VideoCapture capture;
    if (!capture.open(video_path_.toStdString())) {
        emit ProcessingStopped();
        running_ = false;
        return;
    }

    emit ProcessingStarted();

    total_frames_ = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_COUNT));
    double video_fps = capture.get(cv::CAP_PROP_FPS);
    if (video_fps <= 0) video_fps = 24.0;
    if (pipeline_) pipeline_->SetVideoFps(video_fps);

    processed_frames_ = 0;
    face_count_total_ = 0;
    match_count_total_ = 0;
    analysis_success_count_ = 0;
    latest_similarity_ = 0.0f;
    peak_similarity_ = 0.0f;
    det_rec_latency_sum_ = 0.0;
    analysis_latency_sum_ = 0.0;
    elapsed_.start();

    cv::Mat frame;
    while (running_) {
        if (paused_ && !step_requested_) {
            emit ProcessingPaused();
            while (paused_ && !step_requested_ && running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (!running_) break;
            emit ProcessingStarted();
        }
        step_requested_ = false;

        if (!capture.read(frame) || frame.empty()) break;

        int face_count = 0;
        int match_count = 0;
        int analysis_ok = 0;

        if (pipeline_) {
            processed_frames_++;
            FrameResult result = pipeline_->ProcessFrame(frame, processed_frames_);
            face_count = result.face_count;
            for (const auto& m : result.match_results) {
                if (m.is_match) match_count++;
                if (m.similarity > peak_similarity_)
                    peak_similarity_ = m.similarity;
            }
            if (!result.annotated_frame.empty())
                frame = result.annotated_frame;

            for (const auto& m : result.match_results) {
                if (m.is_match && m.similarity > latest_similarity_)
                    latest_similarity_ = m.similarity;
            }

            det_rec_latency_sum_ += result.recognition_latency_ms;
            analysis_latency_sum_ += result.analysis_latency_ms;
            analysis_ok = 1;
        }

        face_count_total_ += face_count;
        match_count_total_ += match_count;
        analysis_success_count_ += analysis_ok;

        double elapsed = elapsed_.elapsed() / 1000.0;
        double avgFps = (elapsed > 0) ? processed_frames_ / elapsed : 0.0;
        double detRecFps = (det_rec_latency_sum_ > 0)
            ? (processed_frames_ * 1000.0 / det_rec_latency_sum_) : 0.0;
        double analysisFps = (analysis_latency_sum_ > 0)
            ? (processed_frames_ * 1000.0 / analysis_latency_sum_) : 0.0;

        cv::Mat bgr;
        if (!frame.empty())
            bgr = frame.clone();

        emit FrameProcessed(bgr, processed_frames_, total_frames_,
                            avgFps, detRecFps, analysisFps,
                            face_count_total_, match_count_total_, analysis_success_count_,
                            latest_similarity_, peak_similarity_);
    }

    capture.release();
    running_ = false;

    // Finalize output video writer so the AVI index is written before
    // the task record (and its output_video_path) is saved.
    if (pipeline_) pipeline_->FinalizeOutput();

    if (processed_frames_ > 0)
        emit ProcessingFinished();
    else
        emit ProcessingStopped();
}

// ═══════════════════════════════════════════════════════════════
// ProcessingPage
// ═══════════════════════════════════════════════════════════════

ProcessingPage::ProcessingPage(std::shared_ptr<db::VideoRepo> video_repo,
                               std::shared_ptr<db::ImageRepo> image_repo,
                               QWidget* parent)
    : QWidget(parent), video_repo_(std::move(video_repo)), image_repo_(std::move(image_repo)) {
    ResolveConfigPaths(config_);
    SetupUI();
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged,
            this, &ProcessingPage::ReapplyTheme);
}

ProcessingPage::~ProcessingPage() {
    if (worker_) {
        worker_->Stop();
        delete worker_;
    }
}

// ─── Helper: analysis backend display name ────────────────────

QString ProcessingPage::AnalysisBackendDisplayName() const {
    switch (config_.analysis_backend) {
    case AnalysisBackend::kOpenFace:  return QStringLiteral("OpenFace");
    case AnalysisBackend::kMediaPipe: return QStringLiteral("MediaPipe");
    case AnalysisBackend::kL2cs:      return QStringLiteral("L2CS-Net");
    case AnalysisBackend::kMefargAu:  return QStringLiteral("MEFARG-AU");
    case AnalysisBackend::kNone:      return QStringLiteral("—");
    }
    return QStringLiteral("—");
}

void ProcessingPage::UpdateAnalysisFpsLabel() {
    if (analysis_fps_title_label_) {
        QString name = AnalysisBackendDisplayName();
        if (config_.analysis_backend == AnalysisBackend::kNone) {
            analysis_fps_title_label_->setText(QStringLiteral("分析 FPS:"));
        } else {
            analysis_fps_title_label_->setText(name + QStringLiteral(" FPS:"));
        }
    }
}

// ─── CreateMediaPanel ─────────────────────────────────────────

QWidget* ProcessingPage::CreateMediaPanel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(270);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto& tm = ThemeManager::Instance();

    // ── Video section ─────────────────────────────────────────
    auto* video_grp = new QGroupBox(QStringLiteral("视频预览"), panel);
    video_grp_ = video_grp;
    video_grp->setStyleSheet(PanelStyle());
    auto* vg_layout = new QVBoxLayout(video_grp);
    vg_layout->setContentsMargins(6, 12, 6, 6);
    vg_layout->setSpacing(2);

    media_video_placeholder_ = new QLabel(QStringLiteral("未选择视频"), video_grp);
    media_video_placeholder_->setAlignment(Qt::AlignCenter);
    media_video_placeholder_->setMinimumHeight(100);
    media_video_placeholder_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 12px; background: %2;"
        "  border: 1px dashed %3; border-radius: 4px; }")
        .arg(tm.TextDim(), tm.DeepBg(), tm.Border()));
    vg_layout->addWidget(media_video_placeholder_);

    media_video_player_ = new VideoPlayerWidget(video_grp);
    media_video_player_->setVisible(false);
    media_video_player_->setMinimumHeight(150);
    vg_layout->addWidget(media_video_player_);

    // Video info rows
    auto addVideoInfo = [&](const QString& label, QLabel*& val) {
        auto* row = new QHBoxLayout();
        row->setSpacing(4);
        auto* title = new QLabel(label, video_grp);
        title->setStyleSheet(StatTitleStyle());
        row->addWidget(title);
        val = new QLabel(QStringLiteral("—"), video_grp);
        val->setStyleSheet(StatValueStyle());
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(val);
        vg_layout->addLayout(row);
    };
    addVideoInfo(QStringLiteral("名称:"), video_info_name_);
    addVideoInfo(QStringLiteral("时长:"), video_info_duration_);
    addVideoInfo(QStringLiteral("分辨率:"), video_info_resolution_);
    addVideoInfo(QStringLiteral("帧率:"), video_info_fps_);
    addVideoInfo(QStringLiteral("大小:"), video_info_size_);

    layout->addWidget(video_grp);

    // ── Image section ─────────────────────────────────────────
    auto* image_grp = new QGroupBox(QStringLiteral("参考图片"), panel);
    image_grp_ = image_grp;
    image_grp->setStyleSheet(PanelStyle());
    auto* ig_layout = new QVBoxLayout(image_grp);
    ig_layout->setContentsMargins(6, 12, 6, 6);
    ig_layout->setSpacing(2);

    media_image_placeholder_ = new QLabel(QStringLiteral("未选择图片"), image_grp);
    media_image_placeholder_->setAlignment(Qt::AlignCenter);
    media_image_placeholder_->setMinimumHeight(80);
    media_image_placeholder_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 12px; background: %2;"
        "  border: 1px dashed %3; border-radius: 4px; }")
        .arg(tm.TextDim(), tm.DeepBg(), tm.Border()));
    ig_layout->addWidget(media_image_placeholder_);

    media_image_label_ = new QLabel(image_grp);
    media_image_label_->setVisible(false);
    media_image_label_->setAlignment(Qt::AlignCenter);
    media_image_label_->setMinimumHeight(80);
    media_image_label_->setStyleSheet(
        QStringLiteral("QLabel { background: %1; border: 1px solid %2; border-radius: 4px; }")
        .arg(tm.DeepBg(), tm.Border()));
    ig_layout->addWidget(media_image_label_);

    // Image info rows
    auto addImageInfo = [&](const QString& label, QLabel*& val) {
        auto* row = new QHBoxLayout();
        row->setSpacing(4);
        auto* title = new QLabel(label, image_grp);
        title->setStyleSheet(StatTitleStyle());
        row->addWidget(title);
        val = new QLabel(QStringLiteral("—"), image_grp);
        val->setStyleSheet(StatValueStyle());
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(val);
        ig_layout->addLayout(row);
    };
    addImageInfo(QStringLiteral("名称:"), image_info_name_);
    addImageInfo(QStringLiteral("置信度:"), image_info_confidence_);
    addImageInfo(QStringLiteral("尺寸:"), image_info_dimensions_);

    layout->addWidget(image_grp, 1);
    layout->addStretch();

    return panel;
}

// ─── CreateStatsPanel ─────────────────────────────────────────

QGroupBox* ProcessingPage::CreateStatsPanel() {
    auto* panel = new QGroupBox(QStringLiteral("实时统计"), this);
    panel->setFixedWidth(270);
    panel->setStyleSheet(PanelStyle());

    auto* layout = new QVBoxLayout(panel);
    layout->setSpacing(4);
    layout->setContentsMargins(10, 14, 10, 8);

    auto addSection = [&](const QString& title) {
        auto* hdr = new QLabel(title, panel);
        hdr->setStyleSheet(SectionHeaderStyle());
        layout->addWidget(hdr);
    };

    auto addRow = [&](const QString& label, QLabel*& valueLabel) {
        auto* row = new QHBoxLayout();
        row->setSpacing(4);
        auto* title = new QLabel(label, panel);
        title->setStyleSheet(StatTitleStyle());
        row->addWidget(title);
        row->addStretch();
        valueLabel = new QLabel(QStringLiteral("—"), panel);
        valueLabel->setStyleSheet(StatValueStyle());
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(valueLabel);
        layout->addLayout(row);
    };

    // ── 性能 ─────────────────────────────────────────────────
    addSection(QStringLiteral("性能"));
    addRow(QStringLiteral("流水线 FPS:"), pipeline_fps_label_);
    addRow(QStringLiteral("检测识别 FPS:"), det_rec_fps_label_);

    {
        auto* row = new QHBoxLayout();
        row->setSpacing(4);
        analysis_fps_title_label_ = new QLabel(QStringLiteral("OpenFace FPS:"), panel);
        analysis_fps_title_label_->setStyleSheet(StatTitleStyle());
        row->addWidget(analysis_fps_title_label_);
        row->addStretch();
        analysis_fps_label_ = new QLabel(QStringLiteral("—"), panel);
        analysis_fps_label_->setStyleSheet(StatValueStyle());
        analysis_fps_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(analysis_fps_label_);
        layout->addLayout(row);
    }

    // ── 进度 ─────────────────────────────────────────────────
    addSection(QStringLiteral("进度"));
    addRow(QStringLiteral("已处理帧数:"), processed_frames_label_);
    addRow(QStringLiteral("检测到人脸:"), face_count_label_);
    addRow(QStringLiteral("匹配目标:"), match_count_label_);
    addRow(QStringLiteral("分析成功:"), analysis_success_label_);

    // ── 质量 ─────────────────────────────────────────────────
    addSection(QStringLiteral("质量"));
    addRow(QStringLiteral("当前相似度:"), current_similarity_label_);
    addRow(QStringLiteral("峰值相似度:"), peak_similarity_label_);

    // ── 时间 ─────────────────────────────────────────────────
    addSection(QStringLiteral("时间"));
    addRow(QStringLiteral("已用时间:"), elapsed_time_label_);

    layout->addStretch();
    return panel;
}

// ─── UpdateMediaPanel ─────────────────────────────────────────

void ProcessingPage::UpdateMediaPanel() {
    // Video
    bool haveVideo = !selected_video_path_.isEmpty();
    media_video_placeholder_->setVisible(!haveVideo);
    media_video_player_->setVisible(haveVideo);
    if (haveVideo) {
        media_video_player_->LoadVideo(selected_video_path_);
        if (auto rec = video_repo_->FindById(selected_video_id_)) {
            video_info_name_->setText(QString::fromStdString(rec->name));
            int totalSec = static_cast<int>(rec->duration);
            int h = totalSec / 3600, m = (totalSec % 3600) / 60, s = totalSec % 60;
            video_info_duration_->setText(
                h > 0 ? QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'))
                      : QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));
            video_info_resolution_->setText(
                QStringLiteral("%1×%2").arg(rec->resolution_width).arg(rec->resolution_height));
            video_info_fps_->setText(QStringLiteral("%1 fps").arg(rec->frame_rate, 0, 'f', 1));
            int64_t bytes = rec->file_size;
            if (bytes < 1024) video_info_size_->setText(QStringLiteral("%1 B").arg(bytes));
            else if (bytes < 1048576) video_info_size_->setText(QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1));
            else if (bytes < 1073741824LL) video_info_size_->setText(QStringLiteral("%1 MB").arg(bytes / 1048576.0, 0, 'f', 1));
            else video_info_size_->setText(QStringLiteral("%1 GB").arg(bytes / 1073741824.0, 0, 'f', 2));
        }
    } else {
        video_info_name_->setText(QStringLiteral("—"));
        video_info_duration_->setText(QStringLiteral("—"));
        video_info_resolution_->setText(QStringLiteral("—"));
        video_info_fps_->setText(QStringLiteral("—"));
        video_info_size_->setText(QStringLiteral("—"));
    }

    // Image
    bool haveImage = !selected_image_path_.isEmpty();
    media_image_placeholder_->setVisible(!haveImage);
    media_image_label_->setVisible(haveImage);
    if (haveImage) {
        QPixmap orig(selected_image_path_);
        if (!orig.isNull()) {
            // Draw face bbox and keypoints if we have embedding
            if (selected_embedding_.valid) {
                QPixmap annotated = orig;
                QPainter painter(&annotated);
                painter.setRenderHint(QPainter::Antialiasing);

                // Look up bbox from DB
                auto rec = image_repo_->FindById(selected_image_id_);
                if (rec) {
                    float iw = static_cast<float>(orig.width());
                    float ih = static_cast<float>(orig.height());
                    int bx1 = static_cast<int>(rec->bbox_x1 * iw);
                    int by1 = static_cast<int>(rec->bbox_y1 * ih);
                    int bx2 = static_cast<int>(rec->bbox_x2 * iw);
                    int by2 = static_cast<int>(rec->bbox_y2 * ih);

                    QPen boxPen(QColor(59, 130, 246), 2);
                    painter.setPen(boxPen);
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(bx1, by1, bx2 - bx1, by2 - by1);

                    // Draw 5 keypoints
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(34, 197, 94));
                    for (int k = 0; k < 5; ++k) {
                        float kx = rec->keypoints[k * 2] * iw;
                        float ky = rec->keypoints[k * 2 + 1] * ih;
                        painter.drawEllipse(QPointF(kx, ky), 3, 3);
                    }
                }
                painter.end();

                QPixmap scaled = annotated.scaled(
                    media_image_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                media_image_label_->setPixmap(scaled);
            } else {
                QPixmap scaled = orig.scaled(
                    media_image_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                media_image_label_->setPixmap(scaled);
            }
        }
        // Image info
        if (auto rec = image_repo_->FindById(selected_image_id_)) {
            image_info_name_->setText(QString::fromStdString(rec->name));
            image_info_confidence_->setText(
                QStringLiteral("%1%").arg(rec->face_confidence * 100.0, 0, 'f', 1));
            QPixmap orig(selected_image_path_);
            image_info_dimensions_->setText(
                orig.isNull() ? QStringLiteral("—")
                              : QStringLiteral("%1×%2").arg(orig.width()).arg(orig.height()));
        }
    } else {
        image_info_name_->setText(QStringLiteral("—"));
        image_info_confidence_->setText(QStringLiteral("—"));
        image_info_dimensions_->setText(QStringLiteral("—"));
    }
}

// ─── SetupUI ──────────────────────────────────────────────────

void ProcessingPage::SetupUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto& tm = ThemeManager::Instance();

    // ── Top bar ───────────────────────────────────────────────
    auto* top_bar = new QHBoxLayout();
    top_bar->setSpacing(12);

    video_btn_ = new QPushButton(QStringLiteral("视频：未选择"), this);
    video_btn_->setStyleSheet(DefaultBtnStyle());
    video_btn_->setCursor(Qt::PointingHandCursor);
    {
        auto* menu = new QMenu(this);
        menu->setStyleSheet(MenuStyle());
        auto* fromFile = menu->addAction(QStringLiteral("从文件中直接选择"));
        auto* fromDb = menu->addAction(QStringLiteral("从数据库中选择"));
        connect(fromFile, &QAction::triggered, this, &ProcessingPage::OnVideoFromFile);
        connect(fromDb, &QAction::triggered, this, &ProcessingPage::OnVideoFromDatabase);
        video_btn_->setMenu(menu);
    }
    top_bar->addWidget(video_btn_);

    image_btn_ = new QPushButton(QStringLiteral("图片：未选择"), this);
    image_btn_->setStyleSheet(DefaultBtnStyle());
    image_btn_->setCursor(Qt::PointingHandCursor);
    {
        auto* menu = new QMenu(this);
        menu->setStyleSheet(MenuStyle());
        auto* fromFile = menu->addAction(QStringLiteral("从文件中直接选择"));
        auto* fromDb = menu->addAction(QStringLiteral("从数据库中选择"));
        connect(fromFile, &QAction::triggered, this, &ProcessingPage::OnImageFromFile);
        connect(fromDb, &QAction::triggered, this, &ProcessingPage::OnImageFromDatabase);
        image_btn_->setMenu(menu);
    }
    top_bar->addWidget(image_btn_);

    backend_btn_ = new QPushButton(QStringLiteral("后端：未选择"), this);
    backend_btn_->setStyleSheet(DefaultBtnStyle());
    backend_btn_->setCursor(Qt::PointingHandCursor);
    {
        auto* menu = new QMenu(this);
        menu->setStyleSheet(MenuStyle());
        const struct { QString label; AnalysisBackend be; } backends[] = {
            {QStringLiteral("OpenFace"),  AnalysisBackend::kOpenFace},
            {QStringLiteral("MediaPipe"), AnalysisBackend::kMediaPipe},
            {QStringLiteral("L2CS-Net"),  AnalysisBackend::kL2cs},
            {QStringLiteral("MEFARG-AU"), AnalysisBackend::kMefargAu},
            {QStringLiteral("None"),      AnalysisBackend::kNone},
        };
        for (const auto& b : backends) {
            auto* action = menu->addAction(b.label);
            action->setData(static_cast<int>(b.be));
        }
        connect(menu, &QMenu::triggered, this, &ProcessingPage::OnBackendSelected);
        backend_btn_->setMenu(menu);
    }
    top_bar->addWidget(backend_btn_);

    top_bar->addStretch();
    root->addLayout(top_bar);

    // ── Two-panel body ─────────────────────────────────────────
    auto* body = new QHBoxLayout();
    body->setSpacing(8);

    // Left column: media (top) + stats (bottom), scrollable
    auto* left_col = new QVBoxLayout();
    left_col->setSpacing(8);
    left_col->setContentsMargins(0, 0, 0, 0);

    auto* left_widget = new QWidget(this);
    auto* left_scroll_layout = new QVBoxLayout(left_widget);
    left_scroll_layout->setContentsMargins(0, 0, 0, 0);
    left_scroll_layout->setSpacing(8);

    media_panel_ = CreateMediaPanel();
    left_scroll_layout->addWidget(media_panel_);
    auto* stats_panel = CreateStatsPanel();
    stats_panel_ = stats_panel;
    left_scroll_layout->addWidget(stats_panel);
    left_scroll_layout->addStretch();

    auto* left_scroll = new QScrollArea(this);
    left_scroll_area_ = left_scroll;
    left_scroll->setWidgetResizable(true);
    left_scroll->setFixedWidth(286);
    left_scroll->setWidget(left_widget);
    left_scroll->setStyleSheet(
        QStringLiteral("QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 6px; background: %1; }")
        .arg(tm.DeepBg()));
    left_col->addWidget(left_scroll);
    body->addLayout(left_col);

    // Center: display + progress bar
    auto* center = new QVBoxLayout();
    center->setSpacing(4);
    display_ = new FrameDisplayWidget(this);
    center->addWidget(display_, 1);

    progress_bar_ = new QProgressBar(this);
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setTextVisible(true);
    progress_bar_->setFixedHeight(18);
    progress_bar_->setFormat(QStringLiteral("就绪"));
    progress_bar_->setStyleSheet(QStringLiteral(
        "QProgressBar { background: %1; border: 1px solid %2; border-radius: 3px;"
        "  text-align: center; color: %3; font-size: 11px; }"
        "QProgressBar::chunk { background: %4; border-radius: 2px; }")
        .arg(tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    center->addWidget(progress_bar_);

    body->addLayout(center, 1);

    root->addLayout(body, 1);

    // ── Control buttons ───────────────────────────────────────
    auto* btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    auto makeBtn = [this, &tm](const QString& text, const QString& color = QString()) {
        auto* btn = new QPushButton(text, this);
        QString style;
        if (!color.isEmpty())
            style = QStringLiteral(
                "QPushButton { background: %1; color: %2; border: none;"
                "  border-radius: 4px; padding: 8px 20px; font-weight: bold; }"
                "QPushButton:hover { background: %1; }"
                "QPushButton:disabled { background: %3; color: %4; }")
                .arg(color, tm.White(), tm.HoverBg(), tm.TextDim());
        else
            style = QStringLiteral(
                "QPushButton { background: %1; color: %2; border: 1px solid %3;"
                "  border-radius: 4px; padding: 8px 20px; }"
                "QPushButton:hover { background: %4; }"
                "QPushButton:disabled { background: %5; color: %6; }")
                .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(),
                     tm.HoverBg(), tm.SurfaceBg(), tm.TextDim());
        btn->setStyleSheet(style);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    start_btn_ = makeBtn(QStringLiteral("开始"), tm.Primary());
    pause_btn_ = makeBtn(QStringLiteral("暂停"));
    resume_btn_ = makeBtn(QStringLiteral("继续"), tm.Success());
    step_btn_ = makeBtn(QStringLiteral("逐帧处理"));
    stop_btn_ = makeBtn(QStringLiteral("停止"));

    view_result_btn_ = makeBtn(QStringLiteral("查看处理结果"), tm.Purple());
    view_result_btn_->setEnabled(false);
    view_result_btn_->setVisible(false);
    connect(view_result_btn_, &QPushButton::clicked, this, [this]() {
        emit ViewTaskResultRequested();
    });

    connect(start_btn_, &QPushButton::clicked, this, &ProcessingPage::OnStart);
    connect(pause_btn_, &QPushButton::clicked, this, &ProcessingPage::OnPause);
    connect(resume_btn_, &QPushButton::clicked, this, &ProcessingPage::OnResume);
    connect(step_btn_, &QPushButton::clicked, this, &ProcessingPage::OnStep);
    connect(stop_btn_, &QPushButton::clicked, this, &ProcessingPage::OnStop);

    btn_layout->addStretch();
    btn_layout->addWidget(start_btn_);
    btn_layout->addWidget(pause_btn_);
    btn_layout->addWidget(resume_btn_);
    btn_layout->addWidget(step_btn_);
    btn_layout->addWidget(stop_btn_);
    btn_layout->addWidget(view_result_btn_);
    btn_layout->addStretch();

    root->addLayout(btn_layout);

    // ── Worker ────────────────────────────────────────────────
    worker_ = new ProcessWorker(this);
    connect(worker_, &ProcessWorker::FrameProcessed,
            this, &ProcessingPage::OnFrameProcessed);
    connect(worker_, &ProcessWorker::ProcessingStarted,
            this, &ProcessingPage::OnWorkerStarted);
    connect(worker_, &ProcessWorker::ProcessingPaused,
            this, &ProcessingPage::OnWorkerPaused);
    connect(worker_, &ProcessWorker::ProcessingStopped,
            this, &ProcessingPage::OnWorkerStopped);
    connect(worker_, &ProcessWorker::ProcessingFinished,
            this, &ProcessingPage::OnWorkerFinished);
    connect(worker_, &ProcessWorker::ProcessingError,
            this, [this](const QString& msg) {
        QMessageBox::critical(this, QStringLiteral("处理错误"), msg);
    });

    // ── Elapsed timer ─────────────────────────────────────────
    elapsed_timer_ = new QTimer(this);
    elapsed_timer_->setInterval(1000);
    connect(elapsed_timer_, &QTimer::timeout, this, &ProcessingPage::UpdateElapsedTime);

    UpdateButtonStates();
    UpdateAnalysisFpsLabel();
}

// ─── Button state management ──────────────────────────────────

void ProcessingPage::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();

    // Re-apply helper styles
    auto selectedStyle = [&]() {
        return QStringLiteral(
            "QPushButton { background: %1; border: 1px solid %2;"
            "  border-radius: 4px; padding: 5px 12px; color: %3; }"
            "QPushButton:hover { border-color: %4; }"
            "QPushButton::menu-indicator { image: none; width: 0px; }")
            .arg(tm.SurfaceBg(), tm.Primary(), tm.TextPrimary(), tm.AccentBlue());
    };
    auto defaultStyle = [&]() {
        return QStringLiteral(
            "QPushButton { background: %1; border: 1px solid %2;"
            "  border-radius: 4px; padding: 5px 12px; color: %3; }"
            "QPushButton:hover { border-color: %4; }"
            "QPushButton::menu-indicator { image: none; width: 0px; }")
            .arg(tm.SurfaceBg(), tm.Border(), tm.TextMuted(), tm.Primary());
    };
    auto panelStyle = [&]() {
        return QStringLiteral(
            "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
            "  border-radius: 4px; margin-top: 10px; padding-top: 14px; background: %3; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }")
            .arg(tm.TextPrimary(), tm.Border(), tm.AltRow());
    };
    auto statTitle = [&]() { return QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(tm.TextMuted()); };
    auto statValue = [&]() { return QStringLiteral("QLabel { color: %1; font-size: 11px; font-weight: bold; }").arg(tm.TextPrimary()); };
    auto sectionHdr = [&]() { return QStringLiteral("QLabel { color: %1; font-size: 9px; font-weight: bold; padding-top: 6px; padding-bottom: 2px; }").arg(tm.TextDim()); };
    auto menuStyle = [&]() {
        return QStringLiteral(
            "QMenu { background: %1; color: %2; border: 1px solid %3; padding: 4px; }"
            "QMenu::item { padding: 6px 24px; }"
            "QMenu::item:selected { background: %4; }")
            .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.SelectedBg());
    };

    // Top bar buttons
    video_btn_->setStyleSheet(selected_video_id_ > 0 ? selectedStyle() : defaultStyle());
    if (video_btn_->menu()) video_btn_->menu()->setStyleSheet(menuStyle());
    image_btn_->setStyleSheet(selected_image_id_ > 0 ? selectedStyle() : defaultStyle());
    if (image_btn_->menu()) image_btn_->menu()->setStyleSheet(menuStyle());
    if (config_.analysis_backend != AnalysisBackend::kNone)
        backend_btn_->setStyleSheet(selectedStyle());
    else
        backend_btn_->setStyleSheet(defaultStyle());
    if (backend_btn_->menu()) backend_btn_->menu()->setStyleSheet(menuStyle());

    // Media panel
    if (video_grp_) video_grp_->setStyleSheet(panelStyle());
    if (image_grp_) image_grp_->setStyleSheet(panelStyle());
    if (stats_panel_) stats_panel_->setStyleSheet(panelStyle());

    media_video_placeholder_->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 12px; background: %2; border: 1px dashed %3; border-radius: 4px; }")
        .arg(tm.TextDim(), tm.DeepBg(), tm.Border()));
    media_image_placeholder_->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 12px; background: %2; border: 1px dashed %3; border-radius: 4px; }")
        .arg(tm.TextDim(), tm.DeepBg(), tm.Border()));
    media_image_label_->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border: 1px solid %2; border-radius: 4px; }")
        .arg(tm.DeepBg(), tm.Border()));

    // Media info labels (re-apply StatTitleStyle / StatValueStyle)
    auto reVideoInfo = [&](QLabel* title, QLabel* value) {
        if (title) title->setStyleSheet(statTitle());
        if (value) value->setStyleSheet(statValue());
    };
    reVideoInfo(nullptr, video_info_name_);
    reVideoInfo(nullptr, video_info_duration_);
    reVideoInfo(nullptr, video_info_resolution_);
    reVideoInfo(nullptr, video_info_fps_);
    reVideoInfo(nullptr, video_info_size_);
    reVideoInfo(nullptr, image_info_name_);
    reVideoInfo(nullptr, image_info_confidence_);
    reVideoInfo(nullptr, image_info_dimensions_);

    // Left scroll
    left_scroll_area_->setStyleSheet(QStringLiteral(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 6px; background: %1; }")
        .arg(tm.DeepBg()));

    // Center display
    display_->ReapplyTheme();

    // Progress bar
    progress_bar_->setStyleSheet(QStringLiteral(
        "QProgressBar { background: %1; border: 1px solid %2; border-radius: 3px;"
        "  text-align: center; color: %3; font-size: 11px; }"
        "QProgressBar::chunk { background: %4; border-radius: 2px; }")
        .arg(tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));

    // Control buttons — re-apply makeBtn logic
    auto restyleBtn = [&](QPushButton* btn, const QString& color) {
        if (!btn) return;
        if (!color.isEmpty())
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; color: %2; border: none;"
                "  border-radius: 4px; padding: 8px 20px; font-weight: bold; }"
                "QPushButton:hover { background: %1; }"
                "QPushButton:disabled { background: %3; color: %4; }")
                .arg(color, tm.White(), tm.HoverBg(), tm.TextDim()));
        else
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; color: %2; border: 1px solid %3;"
                "  border-radius: 4px; padding: 8px 20px; }"
                "QPushButton:hover { background: %4; }"
                "QPushButton:disabled { background: %5; color: %6; }")
                .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(),
                     tm.HoverBg(), tm.SurfaceBg(), tm.TextDim()));
    };
    restyleBtn(start_btn_, tm.Primary());
    restyleBtn(pause_btn_, QString());
    restyleBtn(resume_btn_, tm.Success());
    restyleBtn(step_btn_, QString());
    restyleBtn(stop_btn_, QString());
    restyleBtn(view_result_btn_, tm.Purple());
}

void ProcessingPage::UpdateButtonStates() {
    bool ready = CheckReady();
    switch (state_) {
    case State::Idle:
        start_btn_->setEnabled(ready);
        pause_btn_->setEnabled(false);
        resume_btn_->setEnabled(false);
        step_btn_->setEnabled(false);
        stop_btn_->setEnabled(false);
        if (view_result_btn_) {
            view_result_btn_->setEnabled(false);
            view_result_btn_->setVisible(false);
        }
        break;
    case State::Running:
        start_btn_->setEnabled(false);
        pause_btn_->setEnabled(true);
        resume_btn_->setEnabled(false);
        step_btn_->setEnabled(false);
        stop_btn_->setEnabled(true);
        if (view_result_btn_) {
            view_result_btn_->setEnabled(false);
            view_result_btn_->setVisible(false);
        }
        break;
    case State::Paused:
        start_btn_->setEnabled(false);
        pause_btn_->setEnabled(false);
        resume_btn_->setEnabled(true);
        step_btn_->setEnabled(true);
        stop_btn_->setEnabled(true);
        if (view_result_btn_) {
            view_result_btn_->setEnabled(false);
            view_result_btn_->setVisible(false);
        }
        break;
    case State::Finished:
        start_btn_->setEnabled(ready);
        pause_btn_->setEnabled(false);
        resume_btn_->setEnabled(false);
        step_btn_->setEnabled(false);
        stop_btn_->setEnabled(false);
        if (view_result_btn_) {
            view_result_btn_->setEnabled(true);
            view_result_btn_->setVisible(true);
        }
        break;
    }
}

bool ProcessingPage::CheckReady() {
    return selected_video_id_ > 0 && selected_image_id_ > 0;
}

// ─── Selection slots ──────────────────────────────────────────

void ProcessingPage::OnVideoFromFile() {
    QFileDialog dlg(this, QStringLiteral("选择视频文件"), DataVideoDir(),
        QStringLiteral("视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm);;所有文件 (*.*)"));
    dlg.setDirectory(DataVideoDir());
    dlg.setFileMode(QFileDialog::ExistingFile);
    if (dlg.exec() != QDialog::Accepted) return;
    QString path = dlg.selectedFiles().first();

    QFileInfo fi(path);
    QString baseName = fi.completeBaseName();

    QString fileDir = QDir::cleanPath(fi.absolutePath());
    QString dataDir = QDir::cleanPath(QFileInfo(DataVideoDir()).absoluteFilePath());
    if (QString::compare(fileDir, dataDir, Qt::CaseInsensitive) == 0) {
        bool ok = false;
        int64_t id = baseName.toLongLong(&ok);
        if (ok) {
            auto rec = video_repo_->FindById(id);
            if (rec) {
                SetSelectedVideo(rec->id,
                    QString::fromStdString(rec->name),
                    QString::fromStdString(rec->saved_path));
                return;
            }
        }
    }

    int64_t newId = AutoImportVideo(path);
    if (newId > 0) {
        auto rec = video_repo_->FindById(newId);
        if (rec) {
            QMessageBox::information(this, QStringLiteral("上传视频成功"),
                QStringLiteral("视频 \"%1\" 已成功上传。").arg(QString::fromStdString(rec->name)));
            SetSelectedVideo(rec->id,
                QString::fromStdString(rec->name),
                QString::fromStdString(rec->saved_path));
        }
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("无法导入视频文件，请重试。"));
    }
}

void ProcessingPage::OnVideoFromDatabase() {
    VideoSelectionDialog dlg(video_repo_, this);
    if (dlg.exec() == QDialog::Accepted && dlg.SelectedVideoId() > 0) {
        SetSelectedVideo(dlg.SelectedVideoId(),
                         dlg.SelectedVideoName(),
                         dlg.SelectedVideoPath());
    }
}

void ProcessingPage::OnImageFromFile() {
    QFileDialog dlg(this, QStringLiteral("选择图片文件"), DataImageDir(),
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.bmp *.tiff *.webp);;所有文件 (*.*)"));
    dlg.setDirectory(DataImageDir());
    dlg.setFileMode(QFileDialog::ExistingFile);
    if (dlg.exec() != QDialog::Accepted) return;
    QString path = dlg.selectedFiles().first();

    QFileInfo fi(path);
    QString baseName = fi.completeBaseName();

    QString fileDir = QDir::cleanPath(fi.absolutePath());
    QString dataDir = QDir::cleanPath(QFileInfo(DataImageDir()).absoluteFilePath());
    if (QString::compare(fileDir, dataDir, Qt::CaseInsensitive) == 0) {
        bool ok = false;
        int64_t id = baseName.toLongLong(&ok);
        if (ok) {
            auto rec = image_repo_->FindById(id);
            if (rec) {
                FaceEmbedding emb;
                emb.features = rec->embedding;
                emb.valid = true;
                SetSelectedImage(rec->id, QString::fromStdString(rec->name), emb);
                return;
            }
        }
    }

    int64_t newId = AutoImportImage(path);
    if (newId > 0) {
        auto rec = image_repo_->FindById(newId);
        if (rec) {
            QMessageBox::information(this, QStringLiteral("上传图片成功"),
                QStringLiteral("图片 \"%1\" 已成功上传。").arg(QString::fromStdString(rec->name)));
            FaceEmbedding emb;
            emb.features = rec->embedding;
            emb.valid = true;
            SetSelectedImage(rec->id, QString::fromStdString(rec->name), emb);
        }
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("无法导入图片文件，请重试。"));
    }
}

void ProcessingPage::OnImageFromDatabase() {
    ImageSelectionDialog dlg(image_repo_, this);
    if (dlg.exec() == QDialog::Accepted && dlg.SelectedImageId() > 0) {
        SetSelectedImage(dlg.SelectedImageId(),
                         dlg.SelectedImageName(),
                         dlg.SelectedEmbedding());
    }
}

void ProcessingPage::OnBackendSelected(QAction* action) {
    auto be = static_cast<AnalysisBackend>(action->data().toInt());
    config_.analysis_backend = be;
    backend_btn_->setText(QStringLiteral("后端: %1").arg(action->text()));
    backend_btn_->setStyleSheet(SelectedBtnStyle());
    UpdateAnalysisFpsLabel();
    if (worker_)
        worker_->SetConfig(config_);
}

// ─── Auto-import helpers ──────────────────────────────────────

int64_t ProcessingPage::AutoImportVideo(const QString& sourcePath) {
    QFileInfo fi(sourcePath);
    if (!fi.exists()) return 0;

    int64_t newId = db::Snowflake::Instance().NextId();
    QString dstPath = DataVideoDir() + QStringLiteral("/%1.%2")
        .arg(newId).arg(fi.suffix());

    if (!QFile::copy(sourcePath, dstPath))
        return 0;

    db::VideoRecord rec;
    rec.id = newId;
    rec.name = fi.completeBaseName().toStdString();
    rec.description.clear();
    rec.notes.clear();
    rec.file_size = fi.size();
    rec.saved_path = dstPath.toStdString();
    rec.record_time = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")).toStdString();
    rec.record_location.clear();
    rec.created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();

    cv::VideoCapture cap(dstPath.toStdString());
    if (cap.isOpened()) {
        rec.duration = cap.get(cv::CAP_PROP_FRAME_COUNT) / std::max(1.0, cap.get(cv::CAP_PROP_FPS));
        rec.resolution_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        rec.resolution_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        rec.frame_rate = static_cast<float>(cap.get(cv::CAP_PROP_FPS));
        cap.release();
    } else {
        rec.duration = 0;
        rec.resolution_width = 0;
        rec.resolution_height = 0;
        rec.frame_rate = 0.0f;
    }

    try {
        return video_repo_->Insert(rec);
    } catch (const std::exception& e) {
        spdlog::error("AutoImportVideo failed: {}", e.what());
        QFile::remove(dstPath);
        return 0;
    }
}

int64_t ProcessingPage::AutoImportImage(const QString& sourcePath) {
    QFileInfo fi(sourcePath);
    if (!fi.exists()) return 0;

    int64_t tmpId = db::Snowflake::Instance().NextId();
    QString dstPath = DataImageDir() + QStringLiteral("/%1.%2")
        .arg(tmpId).arg(fi.suffix());

    if (!QFile::copy(sourcePath, dstPath))
        return 0;

    cv::Mat image = cv::imread(dstPath.toStdString());
    if (image.empty()) {
        QFile::remove(dstPath);
        return 0;
    }

    std::vector<BoundingBox> faces;
    try {
        std::string detModel = ResolveModelPath(QStringLiteral("models/buffalo_sc/det_500m.onnx")).toStdString();
        if (!QFile::exists(QString::fromStdString(detModel))) {
            QFile::remove(dstPath);
            return 0;
        }
        detection::FaceDetector::Config detCfg;
        detCfg.confidence_threshold = 0.5f;
        detCfg.nms_threshold = 0.4f;
        detection::FaceDetector detector(detModel, detCfg);
        faces = detector.Detect(image);
    } catch (const std::exception&) {
        QFile::remove(dstPath);
        return 0;
    } catch (...) {
        QFile::remove(dstPath);
        return 0;
    }

    if (faces.empty()) {
        QFile::remove(dstPath);
        return 0;
    }

    auto bestIt = std::max_element(faces.begin(), faces.end(),
        [](const BoundingBox& a, const BoundingBox& b) {
            return a.confidence < b.confidence;
        });
    const BoundingBox& bestBox = *bestIt;

    FaceEmbedding embedding;
    try {
        std::string recModel = ResolveModelPath(QStringLiteral("models/buffalo_sc/w600k_mbf.onnx")).toStdString();
        if (!QFile::exists(QString::fromStdString(recModel))) {
            QFile::remove(dstPath);
            return 0;
        }
        recognition::FaceRecognizer::Config recCfg;
        recognition::FaceRecognizer recognizer(recModel, recCfg);

        cv::Mat aligned = recognition::AlignFace(image, bestBox, 112);
        if (aligned.empty()) {
            QFile::remove(dstPath);
            return 0;
        }
        embedding = recognizer.Extract(aligned);
    } catch (const std::exception&) {
        QFile::remove(dstPath);
        return 0;
    } catch (...) {
        QFile::remove(dstPath);
        return 0;
    }

    if (!embedding.valid) {
        QFile::remove(dstPath);
        return 0;
    }

    db::ImageRecord rec;
    rec.id = tmpId;
    rec.name = fi.completeBaseName().toStdString();
    rec.description.clear();
    rec.notes.clear();
    rec.saved_path = dstPath.toStdString();
    rec.face_confidence = bestBox.confidence;
    rec.bbox_x1 = bestBox.x1;
    rec.bbox_y1 = bestBox.y1;
    rec.bbox_x2 = bestBox.x2;
    rec.bbox_y2 = bestBox.y2;
    rec.keypoints = bestBox.keypoints;
    rec.embedding = embedding.features;
    rec.created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();

    try {
        return image_repo_->Insert(rec);
    } catch (const std::exception& e) {
        spdlog::error("AutoImportImage failed: {}", e.what());
        QFile::remove(dstPath);
        return 0;
    }
}

// ─── Public slots ─────────────────────────────────────────────

void ProcessingPage::SetSelectedVideo(int64_t video_id, const QString& name, const QString& path) {
    selected_video_id_ = video_id;
    selected_video_name_ = name;
    selected_video_path_ = path;
    video_btn_->setText(QStringLiteral("视频: %1").arg(name));
    video_btn_->setStyleSheet(SelectedBtnStyle());
    if (worker_)
        worker_->SetVideoPath(path);
    UpdateMediaPanel();
    UpdateButtonStates();
}

void ProcessingPage::SetSelectedImage(int64_t image_id, const QString& name, const FaceEmbedding& embedding) {
    selected_image_id_ = image_id;
    selected_image_name_ = name;
    selected_embedding_ = embedding;

    auto rec = image_repo_->FindById(image_id);
    if (rec) {
        selected_image_path_ = QString::fromStdString(rec->saved_path);
    }

    image_btn_->setText(QStringLiteral("图片: %1").arg(name));
    image_btn_->setStyleSheet(SelectedBtnStyle());
    if (worker_)
        worker_->SetReference(embedding);
    UpdateMediaPanel();
    UpdateButtonStates();
}

void ProcessingPage::SetConfig(const PipelineConfig& config) {
    config_ = config;
    ResolveConfigPaths(config_);
    QString beName = AnalysisBackendDisplayName();
    backend_btn_->setText(QStringLiteral("后端: %1").arg(beName));
    backend_btn_->setStyleSheet(SelectedBtnStyle());
    UpdateAnalysisFpsLabel();
    if (worker_)
        worker_->SetConfig(config_);
}

// ─── Button slots ─────────────────────────────────────────────

void ProcessingPage::OnStart() {
    if (!CheckReady()) return;

    if (selected_video_path_.isEmpty() || !QFile::exists(selected_video_path_)) {
        QMessageBox::warning(this, QStringLiteral("无法开始处理"),
            QStringLiteral("视频文件不存在，请重新选择视频。"));
        return;
    }
    if (selected_image_path_.isEmpty() || !QFile::exists(selected_image_path_)) {
        QMessageBox::warning(this, QStringLiteral("无法开始处理"),
            QStringLiteral("参考图片文件不存在，请重新选择图片。"));
        return;
    }

    state_ = State::Running;
    total_frame_count_ = 0;
    last_processed_frame_ = 0;
    was_processing_ = true;
    UpdateButtonStates();
    elapsed_seconds_ = 0;
    elapsed_timer_->start();

    progress_bar_->setValue(0);
    progress_bar_->setFormat(QStringLiteral("处理中... %p%"));

    worker_->Start();
}

void ProcessingPage::OnPause() {
    state_ = State::Paused;
    UpdateButtonStates();
    elapsed_timer_->stop();
    progress_bar_->setFormat(QStringLiteral("已暂停 %p%"));
    worker_->Pause();
}

void ProcessingPage::OnResume() {
    state_ = State::Running;
    UpdateButtonStates();
    elapsed_timer_->start();
    progress_bar_->setFormat(QStringLiteral("处理中... %p%"));
    worker_->Resume();
}

void ProcessingPage::OnStep() {
    progress_bar_->setFormat(QStringLiteral("逐帧 %p%"));
    worker_->Step();
}

void ProcessingPage::OnStop() {
    state_ = State::Idle;
    UpdateButtonStates();
    elapsed_timer_->stop();
    progress_bar_->setValue(0);
    progress_bar_->setFormat(QStringLiteral("已停止"));
    worker_->Stop();
    ReleaseMediaResources();
}

// ─── Worker callbacks ─────────────────────────────────────────

void ProcessingPage::OnFrameProcessed(cv::Mat bgr, int frameIndex, int totalFrames,
                                       double pipelineFps, double detRecFps, double analysisFps,
                                       int faceCount, int matchCount, int analysisSuccessCount,
                                       float currentSimilarity, float peakSimilarity) {
    display_->SetFrame(bgr);

    // Performance
    pipeline_fps_label_->setText(QStringLiteral("%1").arg(pipelineFps, 0, 'f', 1));
    det_rec_fps_label_->setText(QStringLiteral("%1").arg(detRecFps, 0, 'f', 1));
    analysis_fps_label_->setText(
        config_.analysis_backend == AnalysisBackend::kNone
            ? QStringLiteral("—")
            : QStringLiteral("%1").arg(analysisFps, 0, 'f', 1));

    // Progress
    processed_frames_label_->setText(
        totalFrames > 0
            ? QStringLiteral("%1 / %2").arg(frameIndex).arg(totalFrames)
            : QString::number(frameIndex));
    face_count_label_->setText(QString::number(faceCount));
    match_count_label_->setText(QString::number(matchCount));
    analysis_success_label_->setText(QString::number(analysisSuccessCount));

    // Quality
    current_similarity_label_->setText(QStringLiteral("%1").arg(currentSimilarity, 0, 'f', 3));
    peak_similarity_label_->setText(QStringLiteral("%1").arg(peakSimilarity, 0, 'f', 3));

    // Progress bar
    total_frame_count_ = totalFrames;
    last_processed_frame_ = frameIndex;
    if (totalFrames > 0) {
        int pct = static_cast<int>(100.0 * frameIndex / totalFrames);
        if (pct > 100) pct = 100;
        progress_bar_->setValue(pct);
    }
}

void ProcessingPage::OnWorkerStarted() {
    state_ = State::Running;
    UpdateButtonStates();
    emit ProcessingActive(true);
}

void ProcessingPage::OnWorkerPaused() {
    state_ = State::Paused;
    UpdateButtonStates();
    emit ProcessingActive(false);
}

void ProcessingPage::OnWorkerStopped() {
    emit ProcessingActive(false);
    if (was_processing_) {
        was_processing_ = false;
        QString backend = QString::fromStdString(
            AnalysisBackendToString(config_.analysis_backend));
        QString output_dir = QString::fromStdString(worker_->GetOutputDir());
        int processed = last_processed_frame_;
        double avg_fps = pipeline_fps_label_->text().toDouble();

        emit ProcessingCompleted(selected_video_id_, selected_image_id_,
                                 backend, avg_fps, processed,
                                 output_dir, QStringLiteral("aborted"));
    }
    state_ = State::Finished;
    UpdateButtonStates();
    elapsed_timer_->stop();
    progress_bar_->setValue(0);
    progress_bar_->setFormat(QStringLiteral("已停止"));
    ReleaseMediaResources();

    // Destroy old worker to release pipeline resources (CSV file handles, etc.)
    delete worker_;
    worker_ = new ProcessWorker(this);
    worker_->SetConfig(config_);
    if (!selected_video_path_.isEmpty())
        worker_->SetVideoPath(selected_video_path_);
    if (selected_embedding_.valid)
        worker_->SetReference(selected_embedding_);
    connect(worker_, &ProcessWorker::FrameProcessed, this, &ProcessingPage::OnFrameProcessed);
    connect(worker_, &ProcessWorker::ProcessingStarted, this, &ProcessingPage::OnWorkerStarted);
    connect(worker_, &ProcessWorker::ProcessingPaused, this, &ProcessingPage::OnWorkerPaused);
    connect(worker_, &ProcessWorker::ProcessingStopped, this, &ProcessingPage::OnWorkerStopped);
    connect(worker_, &ProcessWorker::ProcessingFinished, this, &ProcessingPage::OnWorkerFinished);
    connect(worker_, &ProcessWorker::ProcessingError, this, [this](const QString& msg) {
        QMessageBox::critical(this, QStringLiteral("处理错误"), msg);
    });
}

void ProcessingPage::OnWorkerFinished() {
    state_ = State::Finished;
    was_processing_ = false;
    UpdateButtonStates();
    elapsed_timer_->stop();

    progress_bar_->setValue(100);
    progress_bar_->setFormat(QStringLiteral("完成"));

    QString backend = QString::fromStdString(
        AnalysisBackendToString(config_.analysis_backend));
    QString output_dir = QString::fromStdString(worker_->GetOutputDir());

    int total_frames = last_processed_frame_;
    double avg_fps = pipeline_fps_label_->text().toDouble();

    emit ProcessingActive(false);
    emit ProcessingCompleted(selected_video_id_, selected_image_id_,
                             backend, avg_fps, total_frames,
                             output_dir, QStringLiteral("completed"));
    ReleaseMediaResources();

    // Destroy old worker to release pipeline resources (CSV file handles, etc.)
    delete worker_;
    worker_ = new ProcessWorker(this);
    worker_->SetConfig(config_);
    if (!selected_video_path_.isEmpty())
        worker_->SetVideoPath(selected_video_path_);
    if (selected_embedding_.valid)
        worker_->SetReference(selected_embedding_);
    connect(worker_, &ProcessWorker::FrameProcessed, this, &ProcessingPage::OnFrameProcessed);
    connect(worker_, &ProcessWorker::ProcessingStarted, this, &ProcessingPage::OnWorkerStarted);
    connect(worker_, &ProcessWorker::ProcessingPaused, this, &ProcessingPage::OnWorkerPaused);
    connect(worker_, &ProcessWorker::ProcessingStopped, this, &ProcessingPage::OnWorkerStopped);
    connect(worker_, &ProcessWorker::ProcessingFinished, this, &ProcessingPage::OnWorkerFinished);
    connect(worker_, &ProcessWorker::ProcessingError, this, [this](const QString& msg) {
        QMessageBox::critical(this, QStringLiteral("处理错误"), msg);
    });
}

void ProcessingPage::ReleaseMediaResources() {
    if (IsProcessing()) return;
    if (media_video_player_)
        media_video_player_->Stop();
}

bool ProcessingPage::IsVideoLocked(int64_t video_id) const {
    return IsProcessing() && selected_video_id_ == video_id && video_id > 0;
}

bool ProcessingPage::IsImageLocked(int64_t image_id) const {
    return IsProcessing() && selected_image_id_ == image_id && image_id > 0;
}

std::string ProcessingPage::GetWorkerOutputDir() const {
    return worker_ ? worker_->GetOutputDir() : "";
}

std::string ProcessingPage::GetWorkerOutputVideoPath() const {
    return worker_ ? worker_->GetOutputVideoPath() : "";
}

void ProcessingPage::UpdateElapsedTime() {
    elapsed_seconds_++;
    int h = elapsed_seconds_ / 3600;
    int m = (elapsed_seconds_ % 3600) / 60;
    int s = elapsed_seconds_ % 60;
    if (h > 0)
        elapsed_time_label_->setText(QStringLiteral("%1:%2:%3")
            .arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));
    else
        elapsed_time_label_->setText(QStringLiteral("%1:%2")
            .arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));
}

}  // namespace ui
}  // namespace omniface
