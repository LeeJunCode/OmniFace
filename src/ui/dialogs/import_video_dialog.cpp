#include "omniface/ui/dialogs/import_video_dialog.h"
#include "omniface/ui/theme.h"

#include <QApplication>
#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

#include <opencv2/videoio.hpp>

#include "omniface/db/snowflake.h"
#include "omniface/db/video_repo.h"

namespace omniface {
namespace ui {

static QString DataDir() {
    QString dir = QApplication::applicationDirPath() + QStringLiteral("/data/videos");
    QDir().mkpath(dir);
    return dir;
}

ImportVideoDialog::ImportVideoDialog(db::VideoRepo* repo, QWidget* parent)
    : QDialog(parent), repo_(repo) {
    auto& tm = ThemeManager::Instance();
    setWindowTitle(QStringLiteral("导入视频"));
    setMinimumSize(500, 400);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QLabel { color: %2; }").arg(tm.SurfaceBg(), tm.TextPrimary()));

    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);

    // ── File selection ──────────────────────────────────────

    auto* file_layout = new QHBoxLayout();
    path_edit_ = new QLineEdit(this);
    path_edit_->setPlaceholderText(QStringLiteral("选择视频文件..."));
    path_edit_->setReadOnly(true);
    path_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 8px; color: %3; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary()));
    file_layout->addWidget(path_edit_, 1);

    auto* browse_btn = new QPushButton(QStringLiteral("浏览..."), this);
    browse_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 14px; }"
        "QPushButton:hover { background: %4; }").arg("transparent", tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(browse_btn, &QPushButton::clicked, this, &ImportVideoDialog::OnSelectFile);
    file_layout->addWidget(browse_btn);
    root->addLayout(file_layout);

    // ── Metadata display ────────────────────────────────────

    auto* meta_form = new QFormLayout();
    meta_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    meta_form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto makeValueLabel = [this, &tm](const QString& text = {}) {
        auto* label = new QLabel(text, this);
        label->setStyleSheet(QString("QLabel { color: %1; font-weight: normal; }").arg(tm.TextPrimary()));
        return label;
    };

    duration_label_ = makeValueLabel(QStringLiteral("—"));
    resolution_label_ = makeValueLabel(QStringLiteral("—"));
    frame_rate_label_ = makeValueLabel(QStringLiteral("—"));
    file_size_label_ = makeValueLabel(QStringLiteral("—"));

    meta_form->addRow(QStringLiteral("时长:"), duration_label_);
    meta_form->addRow(QStringLiteral("分辨率:"), resolution_label_);
    meta_form->addRow(QStringLiteral("帧率:"), frame_rate_label_);
    meta_form->addRow(QStringLiteral("文件大小:"), file_size_label_);
    root->addLayout(meta_form);

    // ── Editable fields ─────────────────────────────────────

    auto* edit_form = new QFormLayout();
    edit_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    edit_form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto makeEdit = [this, &tm](const QString& placeholder) {
        auto* edit = new QLineEdit(this);
        edit->setPlaceholderText(placeholder);
        edit->setStyleSheet(QString(
            "QLineEdit { background: %1; border: 1px solid %2;"
            "  border-radius: 4px; padding: 6px 8px; color: %3; }"
            "QLineEdit:focus { border-color: %4; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
        return edit;
    };

    name_edit_ = makeEdit(QStringLiteral("视频名称（默认取文件名）"));
    edit_form->addRow(QStringLiteral("视频名称:"), name_edit_);

    description_edit_ = makeEdit(QStringLiteral("视频描述（可选）"));
    edit_form->addRow(QStringLiteral("描述:"), description_edit_);

    notes_edit_ = makeEdit(QStringLiteral("备注（可选）"));
    edit_form->addRow(QStringLiteral("备注:"), notes_edit_);

    record_time_edit_ = new DatePicker(this);
    record_time_edit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    record_time_edit_->setDate(QDate::currentDate());
    edit_form->addRow(QStringLiteral("录制时间:"), record_time_edit_);

    location_edit_ = makeEdit(QStringLiteral("录制地点（可选）"));
    edit_form->addRow(QStringLiteral("录制地点:"), location_edit_);
    root->addLayout(edit_form);

    root->addStretch();

    // ── Buttons ─────────────────────────────────────────────

    auto* btn_layout = new QHBoxLayout();
    btn_layout->addStretch();

    auto* cancel_btn = new QPushButton(QStringLiteral("取消"), this);
    cancel_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 8px 20px; }"
        "QPushButton:hover { background: %3; }").arg("transparent", tm.TextPrimary(), tm.HoverBg()));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    import_btn_ = new QPushButton(QStringLiteral("导入"), this);
    import_btn_->setEnabled(false);
    import_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 8px 20px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:disabled { background: %4; color: %5; }").arg(tm.Primary(), tm.White(), tm.PrimaryHover(), tm.HoverBg(), tm.TextDim()));
    connect(import_btn_, &QPushButton::clicked, this, &ImportVideoDialog::OnImport);
    btn_layout->addWidget(import_btn_);

    root->addLayout(btn_layout);
}

ImportVideoDialog::~ImportVideoDialog() {
    // Clean up temp copy if user cancelled
    if (result() != QDialog::Accepted && !copied_path_.isEmpty()) {
        QFile::remove(copied_path_);
    }
}

void ImportVideoDialog::OnSelectFile() {
    QString path = QFileDialog::getOpenFileName(this,
        QStringLiteral("选择视频文件"), {},
        QStringLiteral("视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    // Clean up previous temp copy
    if (!copied_path_.isEmpty()) {
        QFile::remove(copied_path_);
        copied_path_.clear();
    }

    source_path_ = path;
    path_edit_->setText(path);

    // Default name from original filename
    QFileInfo fi(path);
    name_edit_->setText(fi.completeBaseName());
    source_file_size_ = fi.size();

    // Display file size
    if (source_file_size_ < 1024)
        file_size_label_->setText(QStringLiteral("%1 B").arg(source_file_size_));
    else if (source_file_size_ < 1024 * 1024)
        file_size_label_->setText(QStringLiteral("%1 KB").arg(source_file_size_ / 1024.0, 0, 'f', 1));
    else if (source_file_size_ < 1024LL * 1024 * 1024)
        file_size_label_->setText(QStringLiteral("%1 MB").arg(source_file_size_ / (1024.0 * 1024.0), 0, 'f', 1));
    else
        file_size_label_->setText(QStringLiteral("%1 GB").arg(source_file_size_ / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2));

    // Copy to data directory with a pure ASCII name (Snowflake ID + original extension)
    copied_id_ = db::Snowflake::Instance().NextId();
    QString ext = fi.suffix();
    QString dstName = QStringLiteral("%1.%2").arg(copied_id_).arg(ext);
    copied_path_ = DataDir() + QStringLiteral("/") + dstName;

    if (!QFile::copy(path, copied_path_)) {
        duration_label_->setText(QStringLiteral("无法复制文件"));
        resolution_label_->setText(QStringLiteral("—"));
        frame_rate_label_->setText(QStringLiteral("—"));
        import_btn_->setEnabled(false);
        return;
    }

    // Extract metadata from the ASCII-safe copy
    cv::VideoCapture cap(copied_path_.toStdString());
    if (!cap.isOpened()) {
        duration_label_->setText(QStringLiteral("无法打开"));
        resolution_label_->setText(QStringLiteral("—"));
        frame_rate_label_->setText(QStringLiteral("—"));
        import_btn_->setEnabled(false);
        return;
    }

    cached_fps_ = cap.get(cv::CAP_PROP_FPS);
    cached_width_ = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    cached_height_ = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double frame_count = cap.get(cv::CAP_PROP_FRAME_COUNT);
    cached_duration_ = (cached_fps_ > 0) ? frame_count / cached_fps_ : 0.0;
    cap.release();

    int h = static_cast<int>(cached_duration_) / 3600;
    int m = (static_cast<int>(cached_duration_) % 3600) / 60;
    int s = static_cast<int>(cached_duration_) % 60;
    if (h > 0)
        duration_label_->setText(QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));
    else
        duration_label_->setText(QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));

    resolution_label_->setText(QStringLiteral("%1 × %2").arg(cached_width_).arg(cached_height_));
    frame_rate_label_->setText(QStringLiteral("%1 fps").arg(cached_fps_, 0, 'f', 1));

    import_btn_->setEnabled(cached_width_ > 0 && cached_height_ > 0);
}

void ImportVideoDialog::OnImport() {
    if (source_path_.isEmpty() || copied_path_.isEmpty()) return;

    // Use the already-copied file; metadata already cached
    db::VideoRecord rec;
    rec.id = copied_id_;
    rec.name = name_edit_->text().trimmed().toStdString();
    if (rec.name.empty()) {
        QFileInfo fi(source_path_);
        rec.name = fi.completeBaseName().toStdString();
    }
    rec.description = description_edit_->text().trimmed().toStdString();
    rec.notes = notes_edit_->text().trimmed().toStdString();
    rec.duration = cached_duration_;
    rec.resolution_width = cached_width_;
    rec.resolution_height = cached_height_;
    rec.frame_rate = cached_fps_;
    rec.record_time = record_time_edit_->date().toString(QStringLiteral("yyyy-MM-dd")).toStdString();
    rec.record_location = location_edit_->text().trimmed().toStdString();
    rec.file_size = source_file_size_;
    rec.saved_path = copied_path_.toStdString();
    rec.created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();

    imported_id_ = 0;
    try {
        imported_id_ = repo_->Insert(rec);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("写入数据库失败:\n%1").arg(QString::fromStdString(e.what())));
        return;
    }
    if (imported_id_ > 0) {
        QMessageBox::information(this, QStringLiteral("导入成功"),
            QStringLiteral("视频 \"%1\" 已成功导入。").arg(QString::fromStdString(rec.name)));
        copied_path_.clear();  // Don't clean up — file is now managed
        accept();
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("写入数据库失败。"));
        QFile::remove(copied_path_);
        copied_path_.clear();
    }
}

}  // namespace ui
}  // namespace omniface
