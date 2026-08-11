#include "omniface/ui/pages/video_detail_page.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "omniface/db/video_repo.h"
#include "omniface/ui/theme.h"
#include "omniface/ui/widgets/video_player_widget.h"

namespace omniface {
namespace ui {

VideoDetailPage::VideoDetailPage(std::shared_ptr<db::VideoRepo> repo, QWidget* parent)
    : QWidget(parent), repo_(std::move(repo)) {
    SetupUI();
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged,
            this, &VideoDetailPage::ReapplyTheme);
}

void VideoDetailPage::SetupUI() {
    auto& tm = ThemeManager::Instance();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 8, 16, 16);
    root->setSpacing(8);

    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QStringLiteral("← 返回"), this);
    back_btn_ = back_btn;
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; }"
        "QPushButton:hover { color: %2; }").arg(tm.Primary(), tm.AccentBlue()));
    connect(back_btn, &QPushButton::clicked, this, &VideoDetailPage::BackRequested);
    top_bar->addWidget(back_btn);
    top_bar->addStretch();

    auto* edit_btn = new QPushButton(QStringLiteral("编辑"), this);
    edit_btn_ = edit_btn;
    edit_btn->setCursor(Qt::PointingHandCursor);
    edit_btn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background: %1; }").arg(tm.HoverBg(), tm.TextPrimary()));
    connect(edit_btn, &QPushButton::clicked, this, [this]() {
        if (video_id_ > 0) emit EditRequested(video_id_);
    });
    top_bar->addWidget(edit_btn);
    root->addLayout(top_bar);

    auto* main_area = new QHBoxLayout();
    main_area->setSpacing(12);

    player_ = new VideoPlayerWidget(this);
    player_->setMinimumSize(400, 260);
    main_area->addWidget(player_, 2);

    auto* info_panel = new QGroupBox(QStringLiteral("视频信息"), this);
    info_panel_ = info_panel;
    info_panel->setFixedWidth(280);
    info_panel->setStyleSheet(QString(
        "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
        "  border-radius: 4px; margin-top: 8px; padding-top: 14px; background: %3; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QLabel { color: %1; font-size: 12px; }").arg(tm.TextPrimary(), tm.Border(), tm.AltRow()));
    auto* info_layout = new QFormLayout(info_panel);
    info_layout->setSpacing(8);
    auto addField = [&](const QString& label, QLabel*& value) {
        value = new QLabel(QStringLiteral("—"), info_panel);
        value->setWordWrap(true);
        value->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.TextMuted()));
        info_layout->addRow(label, value);
    };
    addField(QStringLiteral("名称:"), name_label_);
    addField(QStringLiteral("描述:"), description_label_);
    addField(QStringLiteral("备注:"), notes_label_);
    addField(QStringLiteral("时长:"), duration_label_);
    addField(QStringLiteral("分辨率:"), resolution_label_);
    addField(QStringLiteral("帧率:"), fps_label_);
    addField(QStringLiteral("录制时间:"), record_time_label_);
    addField(QStringLiteral("录制地点:"), location_label_);
    addField(QStringLiteral("文件大小:"), file_size_label_);
    addField(QStringLiteral("保存路径:"), saved_path_label_);
    addField(QStringLiteral("上传时间:"), created_at_label_);
    main_area->addWidget(info_panel);
    root->addLayout(main_area, 1);
}

void VideoDetailPage::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();
    back_btn_->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; }"
        "QPushButton:hover { color: %2; }").arg(tm.Primary(), tm.AccentBlue()));
    edit_btn_->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background: %1; }").arg(tm.HoverBg(), tm.TextPrimary()));
    if (info_panel_) {
        info_panel_->setStyleSheet(QString(
            "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
            "  border-radius: 4px; margin-top: 8px; padding-top: 14px; background: %3; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
            "QLabel { color: %1; font-size: 12px; }").arg(tm.TextPrimary(), tm.Border(), tm.AltRow()));
        auto reLabel = [&](QLabel* lbl) {
            if (lbl) lbl->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.TextMuted()));
        };
        reLabel(name_label_);
        reLabel(description_label_);
        reLabel(notes_label_);
        reLabel(duration_label_);
        reLabel(resolution_label_);
        reLabel(fps_label_);
        reLabel(record_time_label_);
        reLabel(location_label_);
        reLabel(file_size_label_);
        reLabel(saved_path_label_);
        reLabel(created_at_label_);
    }
}

void VideoDetailPage::LoadVideo(int64_t video_id) {
    video_id_ = video_id;
    auto rec = repo_->FindById(video_id);
    if (rec) {
        Populate(*rec);
        player_->SetPendingPath(QString::fromStdString(rec->saved_path));
    }
}

void VideoDetailPage::ReleaseResources() {
    if (player_)
        player_->Reset();
}

void VideoDetailPage::Populate(const db::VideoRecord& rec) {
    name_label_->setText(QString::fromStdString(rec.name));
    description_label_->setText(rec.description.empty() ? QStringLiteral("—") : QString::fromStdString(rec.description));
    notes_label_->setText(rec.notes.empty() ? QStringLiteral("—") : QString::fromStdString(rec.notes));

    auto fmtDuration = [](double sec) {
        int total = static_cast<int>(sec);
        int h = total / 3600, m = (total % 3600) / 60, s = total % 60;
        if (h > 0) return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
        return QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    };
    auto fmtSize = [](int64_t bytes) {
        if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
        if (bytes < 1048576) return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        if (bytes < 1073741824LL) return QStringLiteral("%1 MB").arg(bytes / 1048576.0, 0, 'f', 1);
        return QStringLiteral("%1 GB").arg(bytes / 1073741824.0, 0, 'f', 2);
    };

    duration_label_->setText(fmtDuration(rec.duration));
    resolution_label_->setText(QStringLiteral("%1×%2").arg(rec.resolution_width).arg(rec.resolution_height));
    fps_label_->setText(QStringLiteral("%1 fps").arg(rec.frame_rate, 0, 'f', 1));
    record_time_label_->setText(rec.record_time.empty() ? QStringLiteral("—") : QString::fromStdString(rec.record_time));
    location_label_->setText(rec.record_location.empty() ? QStringLiteral("—") : QString::fromStdString(rec.record_location));
    file_size_label_->setText(fmtSize(rec.file_size));
    saved_path_label_->setText(QString::fromStdString(rec.saved_path));
    if (!rec.created_at.empty()) {
        QString ts = QString::fromStdString(rec.created_at);
        ts.replace(QStringLiteral("T"), QStringLiteral(" "));
        int dot = ts.lastIndexOf(QStringLiteral("."));
        if (dot > 0) ts = ts.left(dot);
        created_at_label_->setText(ts);
    } else {
        created_at_label_->setText(QStringLiteral("—"));
    }
}

}  // namespace ui
}  // namespace omniface
