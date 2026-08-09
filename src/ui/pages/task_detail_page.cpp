#include "omniface/ui/pages/task_detail_page.h"

#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QMessageBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <QtConcurrent/QtConcurrent>

#include <opencv2/videoio.hpp>

#include "omniface/core/quality/openface/openface_quality_evaluator.h"
#include "omniface/core/quality/openface/openface_data_cleaner.h"
#include "omniface/db/image_repo.h"
#include "omniface/db/task_repo.h"
#include "omniface/db/video_repo.h"
#include "omniface/ui/theme.h"
#include "omniface/ui/widgets/detail/l2cs_detail_widget.h"
#include "omniface/ui/widgets/detail/mediapipe_detail_widget.h"
#include "omniface/ui/widgets/detail/mefarg_au_detail_widget.h"
#include "omniface/ui/widgets/detail/openface_detail_widget.h"
#include "omniface/ui/widgets/detail/quality_report_widget.h"
#include "omniface/ui/widgets/video_player_widget.h"

namespace omniface {
namespace ui {

namespace {

QString TruncatePath(const QString& path, int maxLen = 60) {
    if (path.length() <= maxLen) return path;
    return QStringLiteral("...") + path.right(maxLen - 3);
}

QString FormatDuration(double seconds) {
    if (seconds < 0) seconds = 0;
    int h = static_cast<int>(seconds) / 3600;
    int m = (static_cast<int>(seconds) % 3600) / 60;
    int s = static_cast<int>(seconds) % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

}  // namespace

TaskDetailPage::TaskDetailPage(std::shared_ptr<db::TaskRepo> task_repo,
                               std::shared_ptr<db::VideoRepo> video_repo,
                               std::shared_ptr<db::ImageRepo> image_repo,
                               QWidget* parent)
    : QWidget(parent), task_repo_(std::move(task_repo)),
      video_repo_(std::move(video_repo)), image_repo_(std::move(image_repo)) {
    SetupUI();
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged,
            this, &TaskDetailPage::ReapplyTheme);
}

void TaskDetailPage::SetupUI() {
    auto& tm = ThemeManager::Instance();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 8, 16, 16);
    root->setSpacing(8);

    // ── Top bar ──────────────────────────────────────────────

    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QStringLiteral("← 返回"), this);
    back_btn_ = back_btn;
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; }"
        "QPushButton:hover { color: %2; }").arg(tm.Primary(), tm.AccentBlue()));
    connect(back_btn, &QPushButton::clicked, this, &TaskDetailPage::BackRequested);
    top_bar->addWidget(back_btn);

    auto* title = new QLabel(QStringLiteral("任务详情"), this);
    title_label_ = title;
    title->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: bold; color: %1; }").arg(tm.TextPrimary()));
    top_bar->addWidget(title);
    top_bar->addStretch();

    auto* edit_btn = new QPushButton(QStringLiteral("编辑"), this);
    edit_btn_ = edit_btn;
    edit_btn->setCursor(Qt::PointingHandCursor);
    edit_btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }").arg(tm.Primary(), tm.White(), tm.PrimaryHover()));
    connect(edit_btn, &QPushButton::clicked, this, [this]() {
        if (task_id_ > 0) emit EditRequested(task_id_);
    });
    top_bar->addWidget(edit_btn);
    root->addLayout(top_bar);

    // ── Main splitter ────────────────────────────────────────

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter_ = splitter;
    splitter->setStyleSheet(QString(
        "QSplitter::handle { background: %1; width: 2px; }").arg(tm.Border()));

    // ── Left: output (processed) video ───────────────────────

    auto* left = new QWidget(this);
    auto* left_layout = new QVBoxLayout(left);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(4);
    output_player_ = new VideoPlayerWidget(left);
    output_player_->setMinimumSize(400, 260);
    output_player_->SetControlsAlwaysVisible(true);
    connect(output_player_, &VideoPlayerWidget::PositionChanged,
            this, &TaskDetailPage::OnOutputVideoPositionChanged);
    left_layout->addWidget(output_player_, 1);

    output_frame_label_ = new QLabel(QStringLiteral("帧号: —"), left);
    output_frame_label_->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; padding: 2px; }").arg(tm.TextMuted()));
    left_layout->addWidget(output_frame_label_);
    splitter->addWidget(left);

    // ── Right: info / analysis ───────────────────────────────

    auto* right = new QWidget(this);
    auto* right_layout = new QVBoxLayout(right);
    right_layout->setContentsMargins(8, 0, 0, 0);
    right_layout->setSpacing(6);

    // Mode buttons + CSV button
    auto* mode_bar = new QHBoxLayout();
    auto makeModeBtn = [this, &tm](const QString& text) {
        auto* btn = new QPushButton(text, this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; border: none;"
            "  border-radius: 4px; padding: 6px 16px; }"
            "QPushButton:hover { background: %1; }"
            "QPushButton:checked { background: %3; border: 1px solid %4; }")
            .arg(tm.HoverBg(), tm.TextPrimary(), tm.SelectedBg(), tm.Primary()));
        return btn;
    };
    raw_btn_ = makeModeBtn(QStringLiteral("原始信息"));
    analysis_btn_ = makeModeBtn(QStringLiteral("处理结果"));
    quality_btn_ = makeModeBtn(QStringLiteral("数据质量评估"));
    clean_eval_btn_ = makeModeBtn(QStringLiteral("清洗质量评估"));
    connect(raw_btn_, &QPushButton::clicked, this, &TaskDetailPage::OnShowRawInfo);
    connect(analysis_btn_, &QPushButton::clicked, this, &TaskDetailPage::OnShowAnalysis);
    connect(quality_btn_, &QPushButton::clicked, this, &TaskDetailPage::OnShowQuality);
    connect(clean_eval_btn_, &QPushButton::clicked, this, &TaskDetailPage::OnShowCleanQuality);
    analysis_btn_->setChecked(true);
    quality_btn_->setEnabled(false);
    clean_eval_btn_->setEnabled(false);
    mode_bar->addWidget(raw_btn_);
    mode_bar->addWidget(analysis_btn_);
    mode_bar->addWidget(quality_btn_);
    mode_bar->addWidget(clean_eval_btn_);
    mode_bar->addStretch();

    clean_btn_ = new QPushButton(QStringLiteral("数据清洗"), this);
    clean_btn_->setCursor(Qt::PointingHandCursor);
    clean_btn_->setEnabled(false);
    clean_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; color: %2; }"
        "QPushButton:disabled { color: #666; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(clean_btn_, &QPushButton::clicked, this, &TaskDetailPage::OnCleanData);
    mode_bar->addWidget(clean_btn_);

    csv_btn_ = new QPushButton(QStringLiteral("查看原始CSV"), this);
    csv_btn_->setCursor(Qt::PointingHandCursor);
    csv_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; color: %2; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(csv_btn_, &QPushButton::clicked, this, [this]() {
        if (task_id_ > 0) emit ViewCsvRequested(task_id_);
    });
    mode_bar->addWidget(csv_btn_);

    clean_csv_btn_ = new QPushButton(QStringLiteral("查看清洗后CSV"), this);
    clean_csv_btn_->setCursor(Qt::PointingHandCursor);
    clean_csv_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; color: %2; }"
        "QPushButton:disabled { color: #666; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    clean_csv_btn_->setEnabled(false);
    connect(clean_csv_btn_, &QPushButton::clicked, this, [this]() {
        if (task_id_ > 0) emit ViewCleanCsvRequested(task_id_);
    });
    mode_bar->addWidget(clean_csv_btn_);
    right_layout->addLayout(mode_bar);

    // ── Raw info panel ──────────────────────────────────────

    raw_info_panel_ = new QWidget(this);
    raw_info_panel_->setVisible(false);
    auto* raw_outer = new QVBoxLayout(raw_info_panel_);
    raw_outer->setContentsMargins(0, 0, 0, 0);
    raw_outer->setSpacing(6);

    raw_scroll_area_ = new QScrollArea(this);
    raw_scroll_area_->setWidget(raw_info_panel_);
    raw_scroll_area_->setWidgetResizable(true);
    raw_scroll_area_->setVisible(false);
    raw_scroll_area_->setStyleSheet(
        QString("QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical {"
        "  background: %1; width: 6px; border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %2; border-radius: 3px; min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: %2; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
        .arg(tm.AltRow(), tm.HoverBg()));

    auto groupStyle = QString(
        "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
        "  border-radius: 4px; margin-top: 8px; padding-top: 14px; background: %3; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }")
        .arg(tm.TextPrimary(), tm.Border(), tm.AltRow());
    auto fieldStyle = QString("QLabel { color: %1; font-size: 11px; }").arg(tm.TextMuted());

    auto addField = [&](QFormLayout* form, const QString& label, QLabel*& value) {
        value = new QLabel(QStringLiteral("—"), raw_info_panel_);
        value->setWordWrap(true);
        value->setStyleSheet(fieldStyle);
        form->addRow(label, value);
    };

    // ── Source video section ──

    auto* video_group = new QGroupBox(QStringLiteral("源视频"), raw_info_panel_);
    video_group->setStyleSheet(groupStyle);
    auto* video_layout = new QVBoxLayout(video_group);

    source_player_ = new VideoPlayerWidget(raw_info_panel_);
    source_player_->setMinimumHeight(180);
    source_player_->setMaximumHeight(240);
    source_player_->SetControlsAlwaysVisible(true);
    connect(source_player_, &VideoPlayerWidget::PositionChanged,
            this, &TaskDetailPage::OnSourceVideoPositionChanged);
    video_layout->addWidget(source_player_);

    source_frame_label_ = new QLabel(QStringLiteral("帧号: —"), raw_info_panel_);
    source_frame_label_->setStyleSheet(QString("QLabel { color: %1; font-size: 10px; padding: 2px; }").arg(tm.TextDim()));
    video_layout->addWidget(source_frame_label_);
    raw_outer->addWidget(video_group);

    // ── Source video info section ──

    auto* video_info_group = new QGroupBox(QStringLiteral("源视频信息"), raw_info_panel_);
    video_info_group->setStyleSheet(groupStyle);
    auto* video_info_form = new QFormLayout(video_info_group);
    video_info_form->setSpacing(3);
    addField(video_info_form, QStringLiteral("名称:"), raw_video_name_);
    addField(video_info_form, QStringLiteral("时长:"), raw_video_duration_);
    addField(video_info_form, QStringLiteral("分辨率:"), raw_video_resolution_);
    addField(video_info_form, QStringLiteral("帧率:"), raw_video_fps_);
    addField(video_info_form, QStringLiteral("录制地点:"), raw_video_location_);
    raw_outer->addWidget(video_info_group);

    // ── Reference image section ──

    auto* image_group = new QGroupBox(QStringLiteral("参考图片"), raw_info_panel_);
    image_group->setStyleSheet(groupStyle);
    auto* image_layout = new QVBoxLayout(image_group);

    source_image_label_ = new QLabel(raw_info_panel_);
    source_image_label_->setAlignment(Qt::AlignCenter);
    source_image_label_->setFixedHeight(180);
    source_image_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    source_image_label_->setStyleSheet(QString(
        "QLabel { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; color: %3; }").arg(tm.DeepBg(), tm.Border(), tm.TextDim()));
    source_image_label_->setText(QStringLiteral("（未加载图片）"));
    image_layout->addWidget(source_image_label_);
    raw_outer->addWidget(image_group);

    // ── Reference image info section ──

    auto* image_info_group = new QGroupBox(QStringLiteral("参考图片信息"), raw_info_panel_);
    image_info_group->setStyleSheet(groupStyle);
    auto* image_info_form = new QFormLayout(image_info_group);
    image_info_form->setSpacing(3);
    addField(image_info_form, QStringLiteral("名称:"), raw_image_name_);
    addField(image_info_form, QStringLiteral("人脸置信度:"), raw_image_confidence_);
    addField(image_info_form, QStringLiteral("人脸框:"), raw_bbox_);
    raw_outer->addWidget(image_info_group);

    // ── Task info section ──

    auto* task_group = new QGroupBox(QStringLiteral("任务信息"), raw_info_panel_);
    task_group->setStyleSheet(groupStyle);
    auto* task_form = new QFormLayout(task_group);
    task_form->setSpacing(3);
    addField(task_form, QStringLiteral("分析后端:"), raw_task_backend_);
    addField(task_form, QStringLiteral("处理帧数:"), raw_task_frames_);
    addField(task_form, QStringLiteral("平均FPS:"), raw_task_fps_);
    addField(task_form, QStringLiteral("任务时间:"), raw_task_time_);
    addField(task_form, QStringLiteral("任务状态:"), raw_task_status_);
    raw_outer->addWidget(task_group);
    raw_outer->addStretch();

    right_layout->addWidget(raw_scroll_area_, 1);

    // ── Analysis panel ───────────────────────────────────────

    analysis_panel_ = new QWidget(this);
    auto* analysis_layout = new QVBoxLayout(analysis_panel_);
    analysis_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->addWidget(analysis_panel_, 1);

    // ── Quality panel ───────────────────────────────────────

    quality_scroll_area_ = new QScrollArea(this);
    quality_scroll_area_->setVisible(false);
    quality_scroll_area_->setWidgetResizable(true);
    quality_scroll_area_->setStyleSheet(raw_scroll_area_->styleSheet());
    quality_widget_ = new QualityReportWidget(quality_scroll_area_);
    quality_scroll_area_->setWidget(quality_widget_);
    right_layout->addWidget(quality_scroll_area_, 1);

    // ── Clean quality panel ──────────────────────────────────

    clean_quality_scroll_area_ = new QScrollArea(this);
    clean_quality_scroll_area_->setVisible(false);
    clean_quality_scroll_area_->setWidgetResizable(true);
    clean_quality_scroll_area_->setStyleSheet(raw_scroll_area_->styleSheet());
    clean_quality_widget_ = new QualityReportWidget(clean_quality_scroll_area_);
    clean_quality_scroll_area_->setWidget(clean_quality_widget_);
    right_layout->addWidget(clean_quality_scroll_area_, 1);

    splitter->addWidget(right);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);
}

void TaskDetailPage::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();

    back_btn_->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; }"
        "QPushButton:hover { color: %2; }").arg(tm.Primary(), tm.AccentBlue()));

    title_label_->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: bold; color: %1; }").arg(tm.TextPrimary()));

    edit_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }").arg(tm.Primary(), tm.White(), tm.PrimaryHover()));

    splitter_->setStyleSheet(QString("QSplitter::handle { background: %1; width: 2px; }").arg(tm.Border()));

    auto modeBtnStyle = [&]() {
        return QString(
            "QPushButton { background: %1; color: %2; border: none;"
            "  border-radius: 4px; padding: 6px 16px; }"
            "QPushButton:hover { background: %1; }"
            "QPushButton:checked { background: %3; border: 1px solid %4; }")
            .arg(tm.HoverBg(), tm.TextPrimary(), tm.SelectedBg(), tm.Primary());
    };
    raw_btn_->setStyleSheet(modeBtnStyle());
    analysis_btn_->setStyleSheet(modeBtnStyle());
    quality_btn_->setStyleSheet(modeBtnStyle());
    clean_eval_btn_->setStyleSheet(modeBtnStyle());

    clean_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; color: %2; }"
        "QPushButton:disabled { color: #666; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.HoverBg()));

    csv_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; color: %2; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    clean_csv_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; color: %2; }"
        "QPushButton:disabled { color: #666; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.HoverBg()));

    output_frame_label_->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; padding: 2px; }").arg(tm.TextMuted()));

    raw_scroll_area_->setStyleSheet(QString(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical {"
        "  background: %1; width: 6px; border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %2; border-radius: 3px; min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: %2; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
        .arg(tm.AltRow(), tm.HoverBg()));

    auto groupStyle = QString(
        "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
        "  border-radius: 4px; margin-top: 8px; padding-top: 14px; background: %3; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }")
        .arg(tm.TextPrimary(), tm.Border(), tm.AltRow());

    for (auto* gb : raw_info_panel_->findChildren<QGroupBox*>())
        gb->setStyleSheet(groupStyle);

    auto fieldStyle = QString("QLabel { color: %1; font-size: 11px; }").arg(tm.TextMuted());
    source_frame_label_->setStyleSheet(QString("QLabel { color: %1; font-size: 10px; padding: 2px; }").arg(tm.TextDim()));
    source_image_label_->setStyleSheet(QString(
        "QLabel { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; color: %3; }").arg(tm.DeepBg(), tm.Border(), tm.TextDim()));

    auto reLabel = [&](QLabel* lbl) {
        if (lbl) lbl->setStyleSheet(fieldStyle);
    };
    reLabel(raw_video_name_);
    reLabel(raw_video_duration_);
    reLabel(raw_video_resolution_);
    reLabel(raw_video_fps_);
    reLabel(raw_video_location_);
    reLabel(raw_image_name_);
    reLabel(raw_image_confidence_);
    reLabel(raw_bbox_);
    reLabel(raw_task_backend_);
    reLabel(raw_task_frames_);
    reLabel(raw_task_fps_);
    reLabel(raw_task_time_);
    reLabel(raw_task_status_);
}

void TaskDetailPage::LoadTask(int64_t task_id) {
    task_id_ = task_id;
    auto task = task_repo_->FindById(task_id);
    if (!task) return;

    PopulateRawInfo(*task);

    // Load output (annotated / tracked) video for the left player.
    auto video = video_repo_->FindById(task->video_id);
    QString output_video_path;
    if (!task->output_video_path.empty() && QFile::exists(QString::fromStdString(task->output_video_path))) {
        output_video_path = QString::fromStdString(task->output_video_path);
    } else if (video) {
        output_video_path = QString::fromStdString(video->saved_path);
    }
    if (!output_video_path.isEmpty())
        output_player_->LoadVideo(output_video_path);

    // Read actual frame rate from the video file being played, not from DB.
    cv::VideoCapture cap(output_video_path.toStdString());
    if (cap.isOpened()) {
        output_video_fps_ = cap.get(cv::CAP_PROP_FPS);
        cap.release();
    }
    if (output_video_fps_ <= 0) output_video_fps_ = 24.0;

    // Load source video into raw info panel
    if (video) {
        source_player_->LoadVideo(QString::fromStdString(video->saved_path));
    }

    // Load reference image into raw info panel
    auto image = image_repo_->FindById(task->image_id);
    if (image && !image->saved_path.empty()) {
        QPixmap pix(QString::fromStdString(image->saved_path));
        if (!pix.isNull()) {
            int maxH = source_image_label_->height();
            if (maxH <= 0) maxH = 180;
            pix = pix.scaled(pix.width() * maxH / pix.height(), maxH,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
            source_image_label_->setPixmap(pix);
        }
    }

    CreateBackendWidget(task->backend);
    analysis_btn_->setChecked(true);
    OnShowAnalysis();

    quality_evaluated_ = false;
    quality_widget_->Clear();
    clean_quality_evaluated_ = false;
    clean_quality_widget_->Clear();

    // Load CSV data into detail widget
    if (detail_widget_ && !task->csv_path.empty()) {
        detail_widget_->SetCsvPath(QString::fromStdString(task->csv_path));
        detail_widget_->LoadCsvData();
        quality_btn_->setEnabled(true);

        QString clean_csv = QString::fromStdString(task->csv_path);
        clean_csv.replace(QStringLiteral(".csv"), QStringLiteral("_clean.csv"));
        bool has_clean = QFile::exists(clean_csv);
        clean_btn_->setEnabled(true);
        clean_eval_btn_->setEnabled(has_clean);
        clean_csv_btn_->setEnabled(has_clean);

        // Run background quality evaluation for per-frame grade display
        if (task->backend == "openface") {
            auto* bg_eval = new quality::OpenFaceQualityEvaluator(nullptr);
            QString main_csv = QString::fromStdString(task->csv_path);
            QString quality_csv = main_csv;
            quality_csv.replace(QStringLiteral(".csv"), QStringLiteral("_quality.csv"));
            bg_eval->LoadCsv(main_csv, quality_csv);

            connect(bg_eval, &quality::OpenFaceQualityEvaluator::Finished,
                    this, [this, bg_eval](const quality::OpenFaceQualityReport& report) {
                std::unordered_map<int, char> grades;
                for (const auto& f : report.frames)
                    grades[f.frame_number] = f.grade;
                auto* ofw = qobject_cast<OpenFaceDetailWidget*>(detail_widget_);
                if (ofw) ofw->SetFrameGrades(grades);
                bg_eval->deleteLater();
            }, Qt::QueuedConnection);

            connect(bg_eval, &quality::OpenFaceQualityEvaluator::Error,
                    this, [bg_eval](const QString&) {
                bg_eval->deleteLater();
            }, Qt::QueuedConnection);

            (void)QtConcurrent::run([bg_eval]() { bg_eval->Evaluate(); });
        }
    } else {
        quality_btn_->setEnabled(false);
        clean_btn_->setEnabled(false);
        clean_eval_btn_->setEnabled(false);
    }

    // Pass output directory (and aligned face dir for OpenFace)
    if (detail_widget_ && !task->output_dir.empty()) {
        detail_widget_->SetOutputDir(QString::fromStdString(task->output_dir));
        if (task->backend == "openface" && video) {
            QFileInfo fi(QString::fromStdString(video->saved_path));
            QString stem = fi.completeBaseName();
            QString alignedDir = QString::fromStdString(task->output_dir)
                                 + QStringLiteral("/") + stem + QStringLiteral("_aligned");
            detail_widget_->SetAlignedFaceDir(alignedDir);
        }
    }

}

void TaskDetailPage::PopulateRawInfo(const db::TaskRecord& task) {
    auto video = video_repo_->FindById(task.video_id);
    if (video) {
        raw_video_name_->setText(QString::fromStdString(video->name));
        raw_video_duration_->setText(FormatDuration(video->duration));
        raw_video_resolution_->setText(
            QStringLiteral("%1×%2").arg(video->resolution_width).arg(video->resolution_height));
        raw_video_fps_->setText(QStringLiteral("%1 fps").arg(video->frame_rate, 0, 'f', 1));
        raw_video_location_->setText(
            video->record_location.empty() ? QStringLiteral("—")
                                           : QString::fromStdString(video->record_location));
    }

    auto image = image_repo_->FindById(task.image_id);
    if (image) {
        raw_image_name_->setText(QString::fromStdString(image->name));
        raw_image_confidence_->setText(
            QStringLiteral("%1%").arg(image->face_confidence * 100.0, 0, 'f', 1));
        raw_bbox_->setText(QStringLiteral("(%1, %2) - (%3, %4)")
            .arg(image->bbox_x1, 0, 'f', 0).arg(image->bbox_y1, 0, 'f', 0)
            .arg(image->bbox_x2, 0, 'f', 0).arg(image->bbox_y2, 0, 'f', 0));
    }

    raw_task_backend_->setText(QString::fromStdString(task.backend));
    raw_task_frames_->setText(QString::number(task.total_frames));
    raw_task_fps_->setText(QStringLiteral("%1").arg(task.avg_fps, 0, 'f', 1));

    if (!task.start_time.empty()) {
        auto fmtTs = [](const std::string& iso) {
            QString s = QString::fromStdString(iso);
            s.replace(QStringLiteral("T"), QStringLiteral(" "));
            int dot = s.lastIndexOf(QStringLiteral("."));
            if (dot > 0) s = s.left(dot);
            return s;
        };
        QString time_str = fmtTs(task.start_time);
        if (!task.end_time.empty())
            time_str += QStringLiteral(" ~ ") + fmtTs(task.end_time);
        raw_task_time_->setText(time_str);
    }

    auto st = task.status;
    if (st == "completed") raw_task_status_->setText(QStringLiteral("已完成"));
    else if (st == "running") raw_task_status_->setText(QStringLiteral("处理中"));
    else if (st == "failed") raw_task_status_->setText(QStringLiteral("失败"));
    else if (st == "aborted") raw_task_status_->setText(QStringLiteral("已中止"));
    else raw_task_status_->setText(QString::fromStdString(st));
}

void TaskDetailPage::CreateBackendWidget(const std::string& backend) {
    delete detail_widget_;
    detail_widget_ = nullptr;

    if (backend == "openface")
        detail_widget_ = new OpenFaceDetailWidget(analysis_panel_);
    else if (backend == "mediapipe")
        detail_widget_ = new MediaPipeDetailWidget(analysis_panel_);
    else if (backend == "l2cs")
        detail_widget_ = new L2csDetailWidget(analysis_panel_);
    else if (backend == "mefarg_au")
        detail_widget_ = new MefargAuDetailWidget(analysis_panel_);

    if (detail_widget_) {
        auto* layout = qobject_cast<QVBoxLayout*>(analysis_panel_->layout());
        if (layout) layout->insertWidget(0, detail_widget_, 1);
    }
}

void TaskDetailPage::OnShowRawInfo() {
    raw_scroll_area_->setVisible(true);
    raw_info_panel_->setVisible(true);
    if (analysis_panel_) analysis_panel_->setVisible(false);
    if (quality_scroll_area_) quality_scroll_area_->setVisible(false);
    if (clean_quality_scroll_area_) clean_quality_scroll_area_->setVisible(false);
    raw_btn_->setChecked(true);
    analysis_btn_->setChecked(false);
    quality_btn_->setChecked(false);
    clean_eval_btn_->setChecked(false);
}

void TaskDetailPage::OnShowAnalysis() {
    raw_scroll_area_->setVisible(false);
    raw_info_panel_->setVisible(false);
    if (analysis_panel_) analysis_panel_->setVisible(true);
    if (quality_scroll_area_) quality_scroll_area_->setVisible(false);
    if (clean_quality_scroll_area_) clean_quality_scroll_area_->setVisible(false);
    raw_btn_->setChecked(false);
    analysis_btn_->setChecked(true);
    quality_btn_->setChecked(false);
    clean_eval_btn_->setChecked(false);
}

void TaskDetailPage::OnShowQuality() {
    SwitchToQualityPanel();

    if (!quality_evaluated_ && detail_widget_) {
        QString main_csv = detail_widget_->CsvPath();
        QString quality_csv = main_csv;
        quality_csv.replace(QStringLiteral(".csv"), QStringLiteral("_quality.csv"));
        RunQualityEvaluation(main_csv, quality_csv, quality_widget_);
    }
}

void TaskDetailPage::OnShowCleanQuality() {
    SwitchToCleanQualityPanel();

    if (!clean_quality_evaluated_ && detail_widget_) {
        QString main_csv = detail_widget_->CsvPath();
        QString clean_csv = main_csv;
        clean_csv.replace(QStringLiteral(".csv"), QStringLiteral("_clean.csv"));
        if (QFile::exists(clean_csv))
            RunQualityEvaluation(clean_csv, QString(), clean_quality_widget_);
    }
}

void TaskDetailPage::SwitchToQualityPanel() {
    raw_scroll_area_->setVisible(false);
    raw_info_panel_->setVisible(false);
    if (analysis_panel_) analysis_panel_->setVisible(false);
    if (quality_scroll_area_) quality_scroll_area_->setVisible(true);
    if (clean_quality_scroll_area_) clean_quality_scroll_area_->setVisible(false);
    raw_btn_->setChecked(false);
    analysis_btn_->setChecked(false);
    quality_btn_->setChecked(true);
    clean_eval_btn_->setChecked(false);
}

void TaskDetailPage::SwitchToCleanQualityPanel() {
    raw_scroll_area_->setVisible(false);
    raw_info_panel_->setVisible(false);
    if (analysis_panel_) analysis_panel_->setVisible(false);
    if (quality_scroll_area_) quality_scroll_area_->setVisible(false);
    if (clean_quality_scroll_area_) clean_quality_scroll_area_->setVisible(true);
    raw_btn_->setChecked(false);
    analysis_btn_->setChecked(false);
    quality_btn_->setChecked(false);
    clean_eval_btn_->setChecked(true);
}

void TaskDetailPage::RunQualityEvaluation(const QString& main_csv,
                                           const QString& quality_csv,
                                           QualityReportWidget* target) {
    target->SetStatusMessage(QStringLiteral("正在评估中..."));
    quality_btn_->setEnabled(false);
    clean_eval_btn_->setEnabled(false);

    auto* eval = new quality::OpenFaceQualityEvaluator(nullptr);
    eval->LoadCsv(main_csv, quality_csv);

    connect(eval, &quality::OpenFaceQualityEvaluator::Finished,
            this, [this, eval, target](const quality::OpenFaceQualityReport& report) {
        target->RenderReport(report);
        if (target == quality_widget_)
            quality_evaluated_ = true;
        else
            clean_quality_evaluated_ = true;
        quality_btn_->setEnabled(true);
        clean_eval_btn_->setEnabled(true);
        eval->deleteLater();
    }, Qt::QueuedConnection);

    connect(eval, &quality::OpenFaceQualityEvaluator::Error,
            this, [this, eval, target](const QString& msg) {
        target->SetStatusMessage(
            QStringLiteral("<p style='color:#F44336;'>评估失败: %1</p>").arg(msg));
        quality_btn_->setEnabled(true);
        clean_eval_btn_->setEnabled(true);
        eval->deleteLater();
    }, Qt::QueuedConnection);

    (void)QtConcurrent::run([eval]() { eval->Evaluate(); });
}

void TaskDetailPage::OnCleanData() {
    if (!detail_widget_) return;

    QString main_csv = detail_widget_->CsvPath();
    QString quality_csv = main_csv;
    quality_csv.replace(QStringLiteral(".csv"), QStringLiteral("_quality.csv"));

    QString clean_csv = main_csv;
    clean_csv.replace(QStringLiteral(".csv"), QStringLiteral("_clean.csv"));

    // ── Build config dialog ───────────────────────────────────

    auto& tm = ThemeManager::Instance();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("数据清洗"));
    dlg.setFixedSize(400, 200);
    dlg.setStyleSheet(
        QString("QDialog { background: %1; }").arg(tm.SurfaceBg()));

    auto* root = new QVBoxLayout(&dlg);
    root->setSpacing(16);
    root->setContentsMargins(24, 20, 24, 20);

    auto* title = new QLabel(QStringLiteral("选择保留的最低质量等级"), &dlg);
    title->setStyleSheet(QString(
        "QLabel { font-size: 14px; font-weight: bold; color: %1; }").arg(tm.TextPrimary()));
    root->addWidget(title);

    auto* desc = new QLabel(
        QStringLiteral("帧质量等级由置信度、姿态、视线、AU 等 6 个维度综合评定。\n"
                       "低于所选等级的帧将被剔除，输出 _clean.csv。"),
        &dlg);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; }").arg(tm.TextMuted()));
    root->addWidget(desc);

    auto* grade_combo = new QComboBox(&dlg);
    grade_combo->addItem(QStringLiteral("S 级 (≥93分) — 仅保留卓越帧"), static_cast<int>(clean::GradeCut::kS));
    grade_combo->addItem(QStringLiteral("A 级 (≥85分) — 保留优秀及以上"), static_cast<int>(clean::GradeCut::kA));
    grade_combo->addItem(QStringLiteral("B 级 (≥70分) — 保留良好及以上（推荐）"), static_cast<int>(clean::GradeCut::kB));
    grade_combo->addItem(QStringLiteral("C 级 (≥50分) — 保留一般及以上"), static_cast<int>(clean::GradeCut::kC));
    grade_combo->addItem(QStringLiteral("不限制 — 保留全部帧"), static_cast<int>(clean::GradeCut::kNone));
    grade_combo->setCurrentIndex(2);  // default: B
    grade_combo->setStyleSheet(QString(
        "QComboBox { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 10px; font-size: 13px; }"
        "QComboBox QAbstractItemView { background: %1; color: %2;"
        "  selection-background-color: %4; }"
        "QComboBox::drop-down { border: none; }")
        .arg(tm.DeepBg(), tm.TextPrimary(), tm.Border(), tm.Primary()));
    root->addWidget(grade_combo);

    root->addStretch();

    auto* btn_row = new QHBoxLayout();
    btn_row->addStretch();

    auto* cancel_btn = new QPushButton(QStringLiteral("取消"), &dlg);
    cancel_btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 8px 24px; }"
        "QPushButton:hover { background: %4; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(cancel_btn, &QPushButton::clicked, &dlg, &QDialog::reject);
    btn_row->addWidget(cancel_btn);

    btn_row->addSpacing(12);

    auto* ok_btn = new QPushButton(QStringLiteral("开始清洗"), &dlg);
    ok_btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 8px 24px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }")
        .arg(tm.Primary(), tm.White(), tm.PrimaryHover()));
    connect(ok_btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btn_row->addWidget(ok_btn);

    btn_row->addStretch();
    root->addLayout(btn_row);

    if (dlg.exec() != QDialog::Accepted) return;

    clean::CleanConfig cfg;
    cfg.min_grade = static_cast<clean::GradeCut>(
        grade_combo->currentData().toInt());

    // ── Execute cleaning ──────────────────────────────────────

    clean_btn_->setText(QStringLiteral("清洗中..."));
    clean_btn_->setEnabled(false);

    auto* cleaner = new clean::OpenFaceDataCleaner(nullptr);
    if (!cleaner->LoadCsv(main_csv, quality_csv)) {
        QMessageBox::warning(this,
            QStringLiteral("数据清洗"),
            QStringLiteral("加载CSV数据失败"));
        clean_btn_->setText(QStringLiteral("数据清洗"));
        clean_btn_->setEnabled(true);
        delete cleaner;
        return;
    }

    connect(cleaner, &clean::OpenFaceDataCleaner::Finished,
            this, [this, clean_csv, cleaner](const clean::CleanResult& result) {
        QMessageBox::information(this,
            QStringLiteral("数据清洗完成"),
            QString::fromStdString(result.summary_text)
                + QStringLiteral("\n清洗文件: %1").arg(clean_csv));
        clean_btn_->setText(QStringLiteral("数据清洗"));
        clean_btn_->setEnabled(true);
        clean_eval_btn_->setEnabled(true);
        clean_eval_btn_->setChecked(false);
        clean_csv_btn_->setEnabled(true);
        clean_quality_evaluated_ = false;
        clean_quality_widget_->Clear();
        cleaner->deleteLater();
    }, Qt::QueuedConnection);

    connect(cleaner, &clean::OpenFaceDataCleaner::Error,
            this, [this, cleaner](const QString& msg) {
        QMessageBox::warning(this,
            QStringLiteral("数据清洗失败"), msg);
        clean_btn_->setText(QStringLiteral("数据清洗"));
        clean_btn_->setEnabled(true);
        cleaner->deleteLater();
    }, Qt::QueuedConnection);

    (void)QtConcurrent::run([cleaner, cfg]() { cleaner->Execute(cfg); });
}

void TaskDetailPage::OnOutputVideoPositionChanged(qint64 ms) {
    int videoFrame = static_cast<int>(ms * output_video_fps_ / 1000.0);
    int csvFrame = videoFrame;
    output_frame_label_->setText(QStringLiteral("帧号: %1").arg(videoFrame));
    if (detail_widget_)
        detail_widget_->UpdateForFrame(csvFrame);
}

void TaskDetailPage::OnSourceVideoPositionChanged(qint64 ms) {
    auto video = video_repo_->FindById(task_id_ > 0
        ? task_repo_->FindById(task_id_)->video_id : 0);
    double fps = 24.0;
    if (video && !video->saved_path.empty()) {
        cv::VideoCapture cap(video->saved_path);
        if (cap.isOpened()) {
            fps = cap.get(cv::CAP_PROP_FPS);
            cap.release();
        }
    }
    if (fps <= 0) fps = 24.0;
    int frame = static_cast<int>(ms * fps / 1000.0);
    source_frame_label_->setText(QStringLiteral("帧号: %1").arg(frame));
}

}  // namespace ui
}  // namespace omniface
