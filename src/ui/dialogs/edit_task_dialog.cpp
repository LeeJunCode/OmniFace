#include "omniface/ui/dialogs/edit_task_dialog.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "omniface/ui/theme.h"

#include "omniface/db/image_repo.h"
#include "omniface/db/task_repo.h"
#include "omniface/db/video_repo.h"

namespace omniface {
namespace ui {

namespace {

QString FormatDateTime(const std::string& iso) {
    if (iso.empty()) return QStringLiteral("—");
    QString s = QString::fromStdString(iso);
    if (s.length() >= 19)
        return s.left(10) + QStringLiteral(" ") + s.mid(11, 8);
    if (s.length() >= 10)
        return s.left(10);
    return s;
}

QString StatusText(const std::string& status) {
    if (status == "completed") return QStringLiteral("已完成");
    if (status == "running")   return QStringLiteral("处理中");
    if (status == "failed")    return QStringLiteral("失败");
    if (status == "aborted")   return QStringLiteral("已中止");
    return QString::fromStdString(status);
}

}  // namespace

EditTaskDialog::EditTaskDialog(db::TaskRepo* task_repo, db::VideoRepo* video_repo,
                               db::ImageRepo* image_repo, int64_t task_id,
                               QWidget* parent)
    : QDialog(parent), task_repo_(task_repo), video_repo_(video_repo),
      image_repo_(image_repo), task_id_(task_id) {
    auto task = task_repo_->FindById(task_id);
    if (!task) {
        QMessageBox::warning(parent, QStringLiteral("错误"), QStringLiteral("未找到该任务记录。"));
        reject();
        return;
    }
    original_ = *task;

    QString video_name, image_name;
    if (auto v = video_repo_->FindById(task->video_id))
        video_name = QString::fromStdString(v->name);
    if (auto img = image_repo_->FindById(task->image_id))
        image_name = QString::fromStdString(img->name);

    SetupUI(*task, video_name, image_name);
}

void EditTaskDialog::SetupUI(const db::TaskRecord& task,
                              const QString& video_name,
                              const QString& image_name) {
    auto& tm = ThemeManager::Instance();
    setWindowTitle(QStringLiteral("编辑任务信息"));
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

    auto* group = new QGroupBox(QStringLiteral("任务信息"), this);
    auto* form = new QFormLayout(group);
    form->setSpacing(10);

    name_edit_ = new QLineEdit(QString::fromStdString(task.name), this);
    form->addRow(QStringLiteral("任务名称:"), name_edit_);

    description_edit_ = new QLineEdit(QString::fromStdString(task.description), this);
    description_edit_->setPlaceholderText(QStringLiteral("任务描述（可选）"));
    form->addRow(QStringLiteral("描述:"), description_edit_);

    notes_edit_ = new QLineEdit(QString::fromStdString(task.notes), this);
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
        lbl->setWordWrap(true);
        info_form->addRow(label, lbl);
    };
    addRO(QStringLiteral("视频名称:"),
          video_name.isEmpty() ? QStringLiteral("—") : video_name);
    addRO(QStringLiteral("图片名称:"),
          image_name.isEmpty() ? QStringLiteral("—") : image_name);
    addRO(QStringLiteral("分析后端:"), QString::fromStdString(task.backend));
    addRO(QStringLiteral("状态:"), StatusText(task.status));
    addRO(QStringLiteral("处理帧数:"),
          task.total_frames > 0 ? QString::number(task.total_frames) : QStringLiteral("—"));
    addRO(QStringLiteral("平均FPS:"),
          task.avg_fps > 0.0 ? QStringLiteral("%1").arg(task.avg_fps, 0, 'f', 1) : QStringLiteral("—"));
    addRO(QStringLiteral("产出内容:"), QString::fromStdString(task.output_types));
    addRO(QStringLiteral("任务时间:"), FormatDateTime(task.start_time));
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
    connect(save_btn, &QPushButton::clicked, this, &EditTaskDialog::OnSave);
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

void EditTaskDialog::OnSave() {
    QString name = name_edit_->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("验证错误"), QStringLiteral("任务名称不能为空。"));
        return;
    }

    db::TaskRecord fields = original_;
    fields.name = name.toStdString();
    fields.description = description_edit_->text().trimmed().toStdString();
    fields.notes = notes_edit_->text().trimmed().toStdString();

    if (task_repo_->Update(task_id_, fields)) {
        QMessageBox::information(this, QStringLiteral("修改成功"),
            QStringLiteral("任务信息已更新。"));
        accept();
    } else {
        QMessageBox::warning(this, QStringLiteral("修改失败"),
            QStringLiteral("更新数据库失败，请重试。"));
    }
}

}  // namespace ui
}  // namespace omniface
