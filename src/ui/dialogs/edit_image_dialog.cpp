#include "omniface/ui/dialogs/edit_image_dialog.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "omniface/ui/theme.h"

#include "omniface/db/image_repo.h"

namespace omniface {
namespace ui {

EditImageDialog::EditImageDialog(db::ImageRepo* repo, int64_t image_id, QWidget* parent)
    : QDialog(parent), repo_(repo), image_id_(image_id) {
    auto rec = repo_->FindById(image_id);
    if (!rec) {
        QMessageBox::warning(parent, QStringLiteral("错误"), QStringLiteral("未找到该图片记录。"));
        reject();
        return;
    }
    original_ = *rec;
    SetupUI(*rec);
}

void EditImageDialog::SetupUI(const db::ImageRecord& rec) {
    auto& tm = ThemeManager::Instance();
    setWindowTitle(QStringLiteral("编辑图片信息"));
    setMinimumWidth(420);
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

    auto* group = new QGroupBox(QStringLiteral("图片信息"), this);
    auto* form = new QFormLayout(group);
    form->setSpacing(10);

    name_edit_ = new QLineEdit(QString::fromStdString(rec.name), this);
    form->addRow(QStringLiteral("图片名称:"), name_edit_);

    description_edit_ = new QLineEdit(QString::fromStdString(rec.description), this);
    description_edit_->setPlaceholderText(QStringLiteral("图片描述（可选）"));
    form->addRow(QStringLiteral("描述:"), description_edit_);

    notes_edit_ = new QLineEdit(QString::fromStdString(rec.notes), this);
    notes_edit_->setPlaceholderText(QStringLiteral("备注（可选）"));
    form->addRow(QStringLiteral("备注:"), notes_edit_);

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
    addRO(QStringLiteral("人脸置信度:"), QStringLiteral("%1%").arg(rec.face_confidence * 100.0, 0, 'f', 1));
    addRO(QStringLiteral("保存路径:"), QString::fromStdString(rec.saved_path));
    addRO(QStringLiteral("上传时间:"), QString::fromStdString(rec.created_at));
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
    connect(save_btn, &QPushButton::clicked, this, &EditImageDialog::OnSave);
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

void EditImageDialog::OnSave() {
    QString name = name_edit_->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("验证错误"), QStringLiteral("图片名称不能为空。"));
        return;
    }

    db::ImageRecord fields = original_;
    fields.name = name.toStdString();
    fields.description = description_edit_->text().trimmed().toStdString();
    fields.notes = notes_edit_->text().trimmed().toStdString();
    if (!repo_->Update(image_id_, fields)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("更新图片信息失败，请重试。"));
        return;
    }
    QMessageBox::information(this, QStringLiteral("保存成功"),
        QStringLiteral("图片 \"%1\" 的信息已更新。").arg(name));
    accept();
}

}  // namespace ui
}  // namespace omniface
