#include "omniface/ui/dialogs/edit_video_dialog.h"

#include <QDate>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "omniface/ui/theme.h"

#include "omniface/db/video_repo.h"

namespace omniface {
namespace ui {

EditVideoDialog::EditVideoDialog(db::VideoRepo* repo, int64_t video_id, QWidget* parent)
    : QDialog(parent), repo_(repo), video_id_(video_id) {
    auto rec = repo_->FindById(video_id);
    if (!rec) {
        QMessageBox::warning(parent, QStringLiteral("错误"), QStringLiteral("未找到该视频记录。"));
        reject();
        return;
    }
    original_ = *rec;
    SetupUI(*rec);
}

void EditVideoDialog::SetupUI(const db::VideoRecord& rec) {
    auto& tm = ThemeManager::Instance();
    setWindowTitle(QStringLiteral("编辑视频信息"));
    setMinimumWidth(480);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QLineEdit {"
        "  background: %3; border: 1px solid %4;"
        "  border-radius: 4px; padding: 6px 8px; color: %2;"
        "}"
        "QLineEdit:focus { border-color: %5; }"
        "QGroupBox { color: %6; border: 1px solid %4;"
        "  border-radius: 4px; margin-top: 8px; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.DeepBg(), tm.Border(), tm.Primary(), tm.TextMuted()));

    auto* root = new QVBoxLayout(this);

    auto* group = new QGroupBox(QStringLiteral("视频信息"), this);
    auto* form = new QFormLayout(group);
    form->setSpacing(10);

    name_edit_ = new QLineEdit(QString::fromStdString(rec.name), this);
    form->addRow(QStringLiteral("视频名称:"), name_edit_);

    description_edit_ = new QLineEdit(QString::fromStdString(rec.description), this);
    description_edit_->setPlaceholderText(QStringLiteral("视频描述（可选）"));
    form->addRow(QStringLiteral("描述:"), description_edit_);

    notes_edit_ = new QLineEdit(QString::fromStdString(rec.notes), this);
    notes_edit_->setPlaceholderText(QStringLiteral("备注（可选）"));
    form->addRow(QStringLiteral("备注:"), notes_edit_);

    record_time_edit_ = new DatePicker(this);
    record_time_edit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    record_time_edit_->setDate(QDate::currentDate());
    if (!rec.record_time.empty()) {
        auto d = QDate::fromString(QString::fromStdString(rec.record_time).left(10), QStringLiteral("yyyy-MM-dd"));
        if (d.isValid()) record_time_edit_->setDate(d);
    }
    form->addRow(QStringLiteral("录制时间:"), record_time_edit_);

    location_edit_ = new QLineEdit(QString::fromStdString(rec.record_location), this);
    location_edit_->setPlaceholderText(QStringLiteral("请输入录制地点"));
    form->addRow(QStringLiteral("录制地点:"), location_edit_);

    root->addWidget(group);
    root->addSpacing(12);

    // Read-only info
    auto* info_group = new QGroupBox(QStringLiteral("其他信息（只读）"), this);
    auto* info_form = new QFormLayout(info_group);
    info_form->setSpacing(6);
    auto addRO = [&](const QString& label, const QString& value) {
        auto* lbl = new QLabel(value, this);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(tm.TextMuted()));
        info_form->addRow(label, lbl);
    };
    addRO(QStringLiteral("分辨率:"), QStringLiteral("%1×%2").arg(rec.resolution_width).arg(rec.resolution_height));
    addRO(QStringLiteral("帧率:"), QStringLiteral("%1 fps").arg(rec.frame_rate, 0, 'f', 1));
    addRO(QStringLiteral("时长:"), QStringLiteral("%1 秒").arg(rec.duration, 0, 'f', 1));
    addRO(QStringLiteral("保存路径:"), QString::fromStdString(rec.saved_path));
    root->addWidget(info_group);
    root->addSpacing(16);

    // Buttons
    auto* btn_layout = new QHBoxLayout();
    btn_layout->addStretch();

    auto* save_btn = new QPushButton(QStringLiteral("保存"), this);
    save_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %3; border: none;"
        "  border-radius: 4px; padding: 8px 24px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }").arg(tm.Primary(), tm.PrimaryHover(), tm.White()));
    connect(save_btn, &QPushButton::clicked, this, &EditVideoDialog::OnSave);
    btn_layout->addWidget(save_btn);

    auto* cancel_btn = new QPushButton(QStringLiteral("取消"), this);
    cancel_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 8px 24px; }"
        "QPushButton:hover { background: %3; }").arg("transparent", tm.TextPrimary(), tm.HoverBg()));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    btn_layout->addStretch();
    root->addLayout(btn_layout);
}

void EditVideoDialog::OnSave() {
    QString name = name_edit_->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("验证错误"), QStringLiteral("视频名称不能为空。"));
        return;
    }

    // Preserve all original fields, only update editable ones
    db::VideoRecord fields = original_;
    fields.name = name.toStdString();
    fields.description = description_edit_->text().trimmed().toStdString();
    fields.notes = notes_edit_->text().trimmed().toStdString();
    fields.record_time = record_time_edit_->date().toString(QStringLiteral("yyyy-MM-dd")).toStdString();
    fields.record_location = location_edit_->text().trimmed().toStdString();

    if (repo_->Update(video_id_, fields)) {
        QMessageBox::information(this, QStringLiteral("修改成功"),
            QStringLiteral("视频信息已更新。"));
        accept();
    } else {
        QMessageBox::warning(this, QStringLiteral("修改失败"),
            QStringLiteral("更新数据库失败，请重试。"));
    }
}

}  // namespace ui
}  // namespace omniface
