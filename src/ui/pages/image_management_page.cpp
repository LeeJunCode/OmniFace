#include "omniface/ui/pages/image_management_page.h"
#include "omniface/ui/pages/processing_page.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include "omniface/db/image_repo.h"
#include "omniface/ui/dialogs/import_image_dialog.h"
#include "omniface/ui/theme.h"

namespace omniface {
namespace ui {

// ═══════════════════════════════════════════════════════════════
// ImageTableModel
// ═══════════════════════════════════════════════════════════════

ImageTableModel::ImageTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void ImageTableModel::SetRecords(std::vector<db::ImageRecord> records) {
    beginResetModel();
    records_ = std::move(records);
    checked_rows_.clear();
    endResetModel();
}

const db::ImageRecord* ImageTableModel::GetRecord(int row) const {
    if (row < 0 || static_cast<size_t>(row) >= records_.size())
        return nullptr;
    return &records_[row];
}

int ImageTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(records_.size());
}

int ImageTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant ImageTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    int row = index.row();
    if (row < 0 || static_cast<size_t>(row) >= records_.size()) return {};

    const auto& rec = records_[row];

    if (role == Qt::CheckStateRole && index.column() == ColCheck) {
        return checked_rows_.contains(row) ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColCheck:      return {};
        case ColName:       return QString::fromStdString(rec.name);
        case ColDescription:return QString::fromStdString(rec.description);
        case ColNotes:      return QString::fromStdString(rec.notes);
        case ColConfidence: return QStringLiteral("%1%").arg(rec.face_confidence * 100.0, 0, 'f', 1);
        case ColSavedPath:  return QString::fromStdString(rec.saved_path);
        case ColCreatedAt:  return FormatTimestamp(rec.created_at);
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColName:
        case ColDescription:
        case ColNotes:
        case ColSavedPath:
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        default:
            return Qt::AlignCenter;
        }
    }

    if (role == Qt::UserRole) {
        return static_cast<qlonglong>(rec.id);
    }

    return {};
}

bool ImageTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.column() != ColCheck || role != Qt::CheckStateRole)
        return false;

    if (value.toInt() == Qt::Checked)
        checked_rows_.insert(index.row());
    else
        checked_rows_.remove(index.row());

    emit dataChanged(index, index, {role});
    return true;
}

Qt::ItemFlags ImageTableModel::flags(const QModelIndex& index) const {
    auto f = QAbstractTableModel::flags(index);
    if (index.column() == ColCheck)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

QVariant ImageTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColCheck:      return {};
    case ColName:       return QStringLiteral("图片名称");
    case ColDescription:return QStringLiteral("描述");
    case ColNotes:      return QStringLiteral("备注");
    case ColConfidence: return QStringLiteral("人脸置信度");
    case ColSavedPath:  return QStringLiteral("保存路径");
    case ColCreatedAt:  return QStringLiteral("上传时间");
    }
    return {};
}

QSet<int64_t> ImageTableModel::CheckedIds() const {
    QSet<int64_t> ids;
    for (int row : checked_rows_) {
        if (row >= 0 && static_cast<size_t>(row) < records_.size())
            ids.insert(records_[row].id);
    }
    return ids;
}

void ImageTableModel::ToggleAll(bool checked) {
    beginResetModel();
    checked_rows_.clear();
    if (checked) {
        for (int i = 0; i < static_cast<int>(records_.size()); ++i)
            checked_rows_.insert(i);
    }
    endResetModel();
}

QString ImageTableModel::FormatTimestamp(const std::string& iso) {
    if (iso.empty()) return QStringLiteral("—");
    QDateTime dt = QDateTime::fromString(QString::fromStdString(iso), Qt::ISODateWithMs);
    if (dt.isValid())
        return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    // Fallback
    QString s = QString::fromStdString(iso);
    s.replace(QStringLiteral("T"), QStringLiteral(" "));
    int dot = s.lastIndexOf(QStringLiteral("."));
    if (dot > 0) s = s.left(dot);
    return s;
}

// ═══════════════════════════════════════════════════════════════
// ImageFilterProxy
// ═══════════════════════════════════════════════════════════════

ImageFilterProxy::ImageFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent) {}

void ImageFilterProxy::SetNameFilter(const QString& text) {
    name_filter_ = text.trimmed();
    beginFilterChange();
    endFilterChange();
}

bool ImageFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    auto* model = qobject_cast<ImageTableModel*>(sourceModel());
    if (!model) return true;

    const auto* rec = model->GetRecord(sourceRow);
    if (!rec) return false;

    if (!name_filter_.isEmpty()) {
        QString name = QString::fromStdString(rec->name);
        if (!name.contains(name_filter_, Qt::CaseInsensitive))
            return false;
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════
// ImageManagementPage
// ═══════════════════════════════════════════════════════════════

ImageManagementPage::ImageManagementPage(std::shared_ptr<db::ImageRepo> repo, QWidget* parent)
    : QWidget(parent), repo_(std::move(repo)) {
    SetupUI();
    LoadImages();
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged,
            this, &ImageManagementPage::ReapplyTheme);
}

void ImageManagementPage::SetupUI() {
    auto& tm = ThemeManager::Instance();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(8);

    // ── Top bar ──────────────────────────────────────────────

    auto* top_bar = new QHBoxLayout();
    top_bar->setSpacing(8);

    search_box_ = new QLineEdit(this);
    search_box_->setPlaceholderText(QStringLiteral("搜索图片名称..."));
    search_box_->setClearButtonEnabled(true);
    search_box_->setFixedWidth(220);
    search_box_->setStyleSheet(QString(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 8px; color: %3;"
        "}"
        "QLineEdit:focus { border-color: %4; }").arg(tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    connect(search_box_, &QLineEdit::textChanged, this, &ImageManagementPage::OnSearchTextChanged);
    top_bar->addWidget(search_box_);

    auto* reset_btn = new QPushButton(QStringLiteral("重置"), this);
    reset_btn_ = reset_btn;
    reset_btn->setCursor(Qt::PointingHandCursor);
    reset_btn->setStyleSheet(QString(
        "QPushButton {"
        "  background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 12px;"
        "}"
        "QPushButton:hover { background: %4; }").arg("transparent", tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(reset_btn, &QPushButton::clicked, this, &ImageManagementPage::OnResetFilters);
    top_bar->addWidget(reset_btn);

    top_bar->addStretch();

    import_btn_ = new QPushButton(QStringLiteral("+ 导入图片"), this);
    import_btn_->setCursor(Qt::PointingHandCursor);
    import_btn_->setStyleSheet(QString(
        "QPushButton {"
        "  background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %3; }"
        "QPushButton:pressed { background: %4; }").arg(tm.Primary(), tm.White(), tm.PrimaryHover(), tm.PrimaryHover()));
    connect(import_btn_, &QPushButton::clicked, this, &ImageManagementPage::OnImportClicked);
    top_bar->addWidget(import_btn_);

    refresh_btn_ = new QPushButton(QStringLiteral("⟳ 刷新"), this);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(QString(
        "QPushButton {"
        "  background: transparent; color: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 16px;"
        "}"
        "QPushButton:hover { background: %3; }").arg(tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(refresh_btn_, &QPushButton::clicked, this, &ImageManagementPage::Refresh);
    top_bar->addWidget(refresh_btn_);

    root->addLayout(top_bar);

    // ── Table ────────────────────────────────────────────────

    model_ = new ImageTableModel(this);
    proxy_ = new ImageFilterProxy(this);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(Qt::DisplayRole);
    proxy_->setDynamicSortFilter(true);

    table_view_ = new QTableView(this);
    table_view_->setModel(proxy_);
    table_view_->setSortingEnabled(true);
    table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_view_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_view_->verticalHeader()->setVisible(false);
    table_view_->setShowGrid(true);

    auto* hdr = table_view_->horizontalHeader();
    hdr->setStretchLastSection(true);
    hdr->setSectionResizeMode(QHeaderView::Interactive);
    hdr->setDefaultAlignment(Qt::AlignCenter);
    hdr->setMinimumSectionSize(60);
    hdr->resizeSection(ImageTableModel::ColCheck,      30);
    hdr->resizeSection(ImageTableModel::ColName,       140);
    hdr->resizeSection(ImageTableModel::ColDescription,120);
    hdr->resizeSection(ImageTableModel::ColNotes,      100);
    hdr->resizeSection(ImageTableModel::ColConfidence, 80);
    hdr->resizeSection(ImageTableModel::ColSavedPath,  200);

    // Header checkbox for select-all
    connect(hdr, &QHeaderView::sectionClicked, this, [this](int logicalIndex) {
        if (logicalIndex == ImageTableModel::ColCheck) {
            bool allChecked = !model_->CheckedIds().isEmpty()
                           && model_->CheckedIds().size() == static_cast<size_t>(model_->rowCount());
            model_->ToggleAll(!allChecked);
            UpdateBatchDeleteButton();
        }
    });

    table_view_->setStyleSheet(QString(
        "QTableView {"
        "  background: %1; border: 1px solid %2;"
        "  gridline-color: %2;"
        "  selection-background-color: %3;"
        "  selection-color: %4;"
        "}"
        "QTableView::item { padding: 6px 8px; }"
        "QHeaderView::section {"
        "  background: %5; color: %6;"
        "  border: none; border-right: 1px solid %2;"
        "  border-bottom: 2px solid %2;"
        "  padding: 6px 8px; font-weight: bold;"
        "}").arg(tm.DeepBg(), tm.Border(), tm.SelectedBg(),
                 tm.TextPrimary(), tm.SurfaceBg(), tm.TextMuted()));

    connect(table_view_, &QTableView::customContextMenuRequested,
            this, &ImageManagementPage::OnContextMenu);
    connect(table_view_, &QTableView::doubleClicked,
            this, &ImageManagementPage::OnDoubleClicked);

    // Track checkbox changes
    connect(model_, &QAbstractItemModel::dataChanged, this, [this]() {
        UpdateBatchDeleteButton();
    });

    root->addWidget(table_view_, 1);

    // ── Bottom bar ───────────────────────────────────────────

    auto* bottom_bar = new QHBoxLayout();

    count_label_ = new QLabel(this);
    count_label_->setStyleSheet(QString("QLabel { color: %1; padding: 2px 0; }").arg(tm.TextDim()));
    bottom_bar->addWidget(count_label_);

    bottom_bar->addStretch();

    batch_delete_btn_ = new QPushButton(QStringLiteral("删除选中"), this);
    batch_delete_btn_->setEnabled(false);
    batch_delete_btn_->setCursor(Qt::PointingHandCursor);
    batch_delete_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:disabled { background: %5; color: %6; }").arg(
            tm.Error(), tm.White(), tm.Border(), tm.ErrorBg(), tm.HoverBg(), tm.TextDim()));
    connect(batch_delete_btn_, &QPushButton::clicked, this, &ImageManagementPage::OnBatchDelete);
    bottom_bar->addWidget(batch_delete_btn_);

    root->addLayout(bottom_bar);

    UpdateCountLabel();
}

void ImageManagementPage::LoadImages() {
    auto records = repo_->FindAll();
    model_->SetRecords(std::move(records));
    UpdateCountLabel();
    UpdateBatchDeleteButton();
}

void ImageManagementPage::UpdateCountLabel() {
    int filtered = proxy_ ? proxy_->rowCount() : 0;
    count_label_->setText(QStringLiteral("共 %1 张图片").arg(filtered));
}

void ImageManagementPage::UpdateBatchDeleteButton() {
    int checked = model_->CheckedIds().size();
    batch_delete_btn_->setEnabled(checked > 0);
    batch_delete_btn_->setText(QStringLiteral("删除选中 (%1)").arg(checked));
}

void ImageManagementPage::Refresh() {
    LoadImages();
}

void ImageManagementPage::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();
    search_box_->setStyleSheet(QString(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 8px; color: %3;"
        "}"
        "QLineEdit:focus { border-color: %4; }").arg(tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    reset_btn_->setStyleSheet(QString(
        "QPushButton {"
        "  background: transparent; color: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 12px;"
        "}"
        "QPushButton:hover { background: %3; }"
    ).arg(tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    import_btn_->setStyleSheet(QString(
        "QPushButton {"
        "  background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %3; }"
        "QPushButton:pressed { background: %4; }"
    ).arg(tm.Primary(), tm.White(), tm.PrimaryHover(), tm.PrimaryHover()));
    refresh_btn_->setStyleSheet(QString(
        "QPushButton {"
        "  background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px;"
        "}"
        "QPushButton:hover { background: %3; }"
    ).arg("transparent", tm.TextPrimary(), tm.HoverBg()));
    table_view_->setStyleSheet(QString(
        "QTableView {"
        "  background: %1; border: 1px solid %2;"
        "  gridline-color: %2;"
        "  selection-background-color: %3;"
        "  selection-color: %4;"
        "}"
        "QTableView::item { padding: 6px 8px; }"
        "QHeaderView::section {"
        "  background: %5; color: %6;"
        "  border: none; border-right: 1px solid %2;"
        "  border-bottom: 2px solid %2;"
        "  padding: 6px 8px; font-weight: bold;"
        "}"
    ).arg(tm.DeepBg(), tm.Border(), tm.SelectedBg(),
          tm.TextPrimary(), tm.SurfaceBg(), tm.TextMuted()));
    count_label_->setStyleSheet(QString("QLabel { color: %1; padding: 2px 0; }").arg(tm.TextDim()));
    batch_delete_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:disabled { background: %5; color: %6; }"
    ).arg(tm.Error(), tm.White(), tm.Border(), tm.ErrorBg(), tm.HoverBg(), tm.TextDim()));
}

// ─── Slots ───────────────────────────────────────────────────

void ImageManagementPage::OnSearchTextChanged(const QString& text) {
    proxy_->SetNameFilter(text);
    UpdateCountLabel();
}

void ImageManagementPage::OnResetFilters() {
    search_box_->clear();
    UpdateCountLabel();
}

void ImageManagementPage::OnImportClicked() {
    ImportImageDialog dlg(repo_.get(), this);
    if (dlg.exec() == QDialog::Accepted)
        Refresh();
}

void ImageManagementPage::TriggerImport() {
    OnImportClicked();
}

int64_t ImageManagementPage::SelectedImageId() const {
    auto indexes = table_view_->selectionModel()->selectedRows();
    if (indexes.isEmpty()) return 0;
    return indexes.first().data(Qt::UserRole).toLongLong();
}

void ImageManagementPage::OnDoubleClicked(const QModelIndex& index) {
    int64_t id = index.data(Qt::UserRole).toLongLong();
    if (id > 0)
        emit ViewDetailsRequested(id);
}

void ImageManagementPage::OnContextMenu(const QPoint& pos) {
    auto index = table_view_->indexAt(pos);
    if (!index.isValid()) return;

    table_view_->selectRow(index.row());
    int64_t img_id = SelectedImageId();
    if (img_id == 0) return;

    QMenu menu(this);
    auto& tm = ThemeManager::Instance();
    menu.setStyleSheet(QString(
        "QMenu { background: %1; color: %2; border: 1px solid %3; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; }"
        "QMenu::item:selected { background: %4; }"
        "QMenu::separator { background: %3; height: 1px; margin: 4px 8px; }").arg(
            tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.SelectedBg()));

    auto* view_action = menu.addAction(QStringLiteral("查看详情"));
    auto* edit_action = menu.addAction(QStringLiteral("编辑图片信息"));
    auto* open_dir_action = menu.addAction(QStringLiteral("打开文件所在位置"));
    menu.addSeparator();
    auto* delete_action = menu.addAction(QStringLiteral("删除"));

    QAction* chosen = menu.exec(table_view_->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == view_action) {
        emit ViewDetailsRequested(img_id);
    } else if (chosen == edit_action) {
        emit EditRequested(img_id);
    } else if (chosen == open_dir_action) {
        auto rec = repo_->FindById(img_id);
        if (rec && !rec->saved_path.empty()) {
            QString path = QDir::toNativeSeparators(QString::fromStdString(rec->saved_path));
            QProcess::startDetached(QStringLiteral("explorer"), {QStringLiteral("/select,"), path});
        }
    } else if (chosen == delete_action) {
        OnDeleteImage();
    }
}

void ImageManagementPage::OnDeleteImage() {
    int64_t img_id = SelectedImageId();
    if (img_id == 0) return;

    auto rec = repo_->FindById(img_id);
    if (!rec) return;

    QString name = QString::fromStdString(rec->name);

    auto result = QMessageBox::question(this,
        QStringLiteral("删除图片"),
        QStringLiteral("确定要删除图片 \"%1\" 吗？\n\n将同时删除记录和文件，此操作不可撤销。").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) return;

    if (process_page_ && process_page_->IsImageLocked(img_id)) {
        QMessageBox::warning(this, QStringLiteral("无法删除"),
            QStringLiteral("该图片正在处理中使用，请先停止处理后再删除。"));
        return;
    }

    if (!repo_->Delete(img_id)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"),
            QStringLiteral("无法删除：该图片有关联的处理任务，请先删除对应任务。"));
        return;
    }

    if (process_page_)
        process_page_->ClearImageSelection();
    emit ImageDeleted(img_id);

    if (!rec->saved_path.empty()) {
        QFile::remove(QString::fromStdString(rec->saved_path));
    }

    Refresh();
    QMessageBox::information(this, QStringLiteral("删除成功"),
        QStringLiteral("图片 \"%1\" 已成功删除。").arg(name));
}

void ImageManagementPage::OnBatchDelete() {
    QSet<int64_t> ids = model_->CheckedIds();
    if (ids.isEmpty()) return;

    int locked = 0;
    for (int64_t id : ids) {
        if (process_page_ && process_page_->IsImageLocked(id))
            locked++;
    }
    if (locked > 0) {
        auto result = QMessageBox::question(this,
            QStringLiteral("批量删除"),
            QStringLiteral("选中的 %1 张图片中有 %2 张正在处理中，将被跳过。\n\n确定删除其余 %3 张图片吗？\n将同时删除记录和文件，此操作不可撤销。")
                .arg(ids.size()).arg(locked).arg(ids.size() - locked),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    } else {
        auto result = QMessageBox::question(this,
            QStringLiteral("批量删除"),
            QStringLiteral("确定要删除选中的 %1 张图片吗？\n\n将同时删除记录和文件，此操作不可撤销。").arg(ids.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    }

    int deleted = 0;
    int skipped = 0;
    for (int64_t id : ids) {
        if (process_page_ && process_page_->IsImageLocked(id)) {
            skipped++;
            continue;
        }
        auto rec = repo_->FindById(id);
        if (repo_->Delete(id)) {
            if (process_page_)
                process_page_->ClearImageSelection();
            emit ImageDeleted(id);
            if (rec && !rec->saved_path.empty()) {
                QFile::remove(QString::fromStdString(rec->saved_path));
            }
            deleted++;
        }
    }
    Refresh();
    QMessageBox::information(this, QStringLiteral("删除完成"),
        QStringLiteral("成功删除 %1/%2 张图片%3。")
            .arg(deleted).arg(ids.size())
            .arg(skipped > 0 ? QStringLiteral("（跳过 %1 张处理中）").arg(skipped) : QString()));
}

void ImageManagementPage::OnEditImage() {
    int64_t img_id = SelectedImageId();
    if (img_id > 0)
        emit EditRequested(img_id);
}

void ImageManagementPage::OnViewDetails() {
    int64_t img_id = SelectedImageId();
    if (img_id > 0)
        emit ViewDetailsRequested(img_id);
}

}  // namespace ui
}  // namespace omniface
