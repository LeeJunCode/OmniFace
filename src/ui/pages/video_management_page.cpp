#include "omniface/ui/pages/video_management_page.h"

#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include "omniface/db/video_repo.h"
#include "omniface/ui/dialogs/import_video_dialog.h"
#include "omniface/ui/theme.h"

namespace omniface {
namespace ui {

// ═══════════════════════════════════════════════════════════════
// VideoTableModel
// ═══════════════════════════════════════════════════════════════

VideoTableModel::VideoTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void VideoTableModel::SetRecords(std::vector<db::VideoRecord> records) {
    beginResetModel();
    records_ = std::move(records);
    checked_rows_.clear();
    endResetModel();
}

const db::VideoRecord* VideoTableModel::GetRecord(int row) const {
    if (row < 0 || static_cast<size_t>(row) >= records_.size())
        return nullptr;
    return &records_[row];
}

int VideoTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(records_.size());
}

int VideoTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant VideoTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    int row = index.row();
    if (row < 0 || static_cast<size_t>(row) >= records_.size()) return {};

    const auto& rec = records_[row];

    if (role == Qt::CheckStateRole && index.column() == ColCheck) {
        return checked_rows_.contains(row) ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColCheck:          return {};
        case ColName:           return QString::fromStdString(rec.name);
        case ColDescription:    return QString::fromStdString(rec.description);
        case ColNotes:          return QString::fromStdString(rec.notes);
        case ColDuration:       return FormatDuration(rec.duration);
        case ColResolution:     return QStringLiteral("%1×%2").arg(rec.resolution_width).arg(rec.resolution_height);
        case ColFrameRate:      return QStringLiteral("%1 fps").arg(rec.frame_rate, 0, 'f', 1);
        case ColRecordTime:     return QString::fromStdString(rec.record_time);
        case ColRecordLocation: return QString::fromStdString(rec.record_location);
        case ColFileSize:       return FormatFileSize(rec.file_size);
        case ColSavedPath:      return QString::fromStdString(rec.saved_path);
        case ColCreatedAt:      return FormatTimestamp(rec.created_at);
        }
    }

    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColCheck) return static_cast<int>(Qt::AlignCenter);
        switch (index.column()) {
        case ColDuration:   return Qt::AlignCenter;
        case ColResolution: return Qt::AlignCenter;
        case ColFrameRate:  return Qt::AlignCenter;
        case ColFileSize:   return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    if (role == Qt::UserRole) {
        return static_cast<qlonglong>(rec.id);
    }

    return {};
}

QVariant VideoTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColCheck:          return {};
    case ColName:           return QStringLiteral("视频名称");
    case ColDescription:    return QStringLiteral("描述");
    case ColNotes:          return QStringLiteral("备注");
    case ColDuration:       return QStringLiteral("时长");
    case ColResolution:     return QStringLiteral("分辨率");
    case ColFrameRate:      return QStringLiteral("帧率");
    case ColRecordTime:     return QStringLiteral("录制时间");
    case ColRecordLocation: return QStringLiteral("录制地点");
    case ColFileSize:       return QStringLiteral("文件大小");
    case ColSavedPath:      return QStringLiteral("保存路径");
    case ColCreatedAt:      return QStringLiteral("上传时间");
    }
    return {};
}

bool VideoTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.column() != ColCheck || role != Qt::CheckStateRole)
        return false;

    if (value.toInt() == Qt::Checked)
        checked_rows_.insert(index.row());
    else
        checked_rows_.remove(index.row());

    emit dataChanged(index, index, {role});
    return true;
}

Qt::ItemFlags VideoTableModel::flags(const QModelIndex& index) const {
    auto f = QAbstractTableModel::flags(index);
    if (index.column() == ColCheck)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

QSet<int64_t> VideoTableModel::CheckedIds() const {
    QSet<int64_t> ids;
    for (int row : checked_rows_) {
        if (row >= 0 && static_cast<size_t>(row) < records_.size())
            ids.insert(records_[row].id);
    }
    return ids;
}

void VideoTableModel::ToggleAll(bool checked) {
    beginResetModel();
    checked_rows_.clear();
    if (checked) {
        for (int i = 0; i < static_cast<int>(records_.size()); ++i)
            checked_rows_.insert(i);
    }
    endResetModel();
}

QString VideoTableModel::FormatDuration(double seconds) {
    if (seconds < 0) seconds = 0;
    auto total = static_cast<int>(seconds);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QStringLiteral("%1:%2")
        .arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

QString VideoTableModel::FormatFileSize(int64_t bytes) {
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QString VideoTableModel::FormatTimestamp(const std::string& iso) {
    if (iso.empty()) return QStringLiteral("—");
    QString s = QString::fromStdString(iso);
    // Convert "2024-01-15T14:30:25.123" → "2024-01-15 14:30:25"
    s.replace(QStringLiteral("T"), QStringLiteral(" "));
    int dot = s.lastIndexOf(QStringLiteral("."));
    if (dot > 0) s = s.left(dot);
    return s;
}

// ═══════════════════════════════════════════════════════════════
// VideoFilterProxy
// ═══════════════════════════════════════════════════════════════

VideoFilterProxy::VideoFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent) {}

void VideoFilterProxy::SetNameFilter(const QString& text) {
    name_filter_ = text.trimmed();
    beginFilterChange();
    endFilterChange();
}

void VideoFilterProxy::SetDateFrom(const QDate& date) {
    date_from_ = date;
    date_filter_active_ = true;
    beginFilterChange();
    endFilterChange();
}

void VideoFilterProxy::SetDateTo(const QDate& date) {
    date_to_ = date;
    date_filter_active_ = true;
    beginFilterChange();
    endFilterChange();
}

bool VideoFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    auto* model = qobject_cast<VideoTableModel*>(sourceModel());
    if (!model) return true;

    const auto* rec = model->GetRecord(sourceRow);
    if (!rec) return false;

    if (!name_filter_.isEmpty()) {
        QString name = QString::fromStdString(rec->name);
        if (!name.contains(name_filter_, Qt::CaseInsensitive))
            return false;
    }

    if (date_filter_active_ && (date_from_.isValid() || date_to_.isValid())) {
        QString rt = QString::fromStdString(rec->record_time);
        QDate recDate;
        if (rt.length() >= 10)
            recDate = QDate::fromString(rt.left(10), QStringLiteral("yyyy-MM-dd"));
        if (!recDate.isValid())
            recDate = QDate::fromString(rt.left(10), QStringLiteral("yyyy/MM/dd"));
        if (recDate.isValid()) {
            if (date_from_.isValid() && recDate < date_from_)
                return false;
            if (date_to_.isValid() && recDate > date_to_)
                return false;
        }
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════
// VideoManagementPage
// ═══════════════════════════════════════════════════════════════

VideoManagementPage::VideoManagementPage(std::shared_ptr<db::VideoRepo> repo, QWidget* parent)
    : QWidget(parent), repo_(std::move(repo)) {
    SetupUI();
    LoadVideos();
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged,
            this, &VideoManagementPage::ReapplyTheme);
}

void VideoManagementPage::SetupUI() {
    auto& tm = ThemeManager::Instance();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(8);

    // ── Top bar ──────────────────────────────────────────────

    auto* top_bar = new QHBoxLayout();
    top_bar->setSpacing(8);

    search_box_ = new QLineEdit(this);
    search_box_->setPlaceholderText(QStringLiteral("搜索视频名称..."));
    search_box_->setClearButtonEnabled(true);
    search_box_->setFixedWidth(220);
    search_box_->setStyleSheet(QString(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 8px; color: %3;"
        "}"
        "QLineEdit:focus { border-color: %4; }"
    ).arg(tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    connect(search_box_, &QLineEdit::textChanged, this, &VideoManagementPage::OnSearchTextChanged);
    top_bar->addWidget(search_box_);

    auto* date_label_from = new QLabel(QStringLiteral("录制时间:"), this);
    date_label_from->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.TextMuted()));
    top_bar->addWidget(date_label_from);

    date_from_ = new DatePicker(this);
    date_from_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    date_from_->setDate(QDate::currentDate());
    date_from_->blockSignals(true);
    date_from_->setSpecialValueText(QStringLiteral("从"));
    date_from_->setDate(QDate::currentDate());
    date_from_->blockSignals(false);
    connect(date_from_, &DatePicker::dateChanged, this, &VideoManagementPage::OnDateFromChanged);
    top_bar->addWidget(date_from_);

    date_to_ = new DatePicker(this);
    date_to_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    date_to_->setDate(QDate::currentDate());
    date_to_->blockSignals(true);
    date_to_->setSpecialValueText(QStringLiteral("至"));
    date_to_->setDate(QDate::currentDate());
    date_to_->blockSignals(false);
    connect(date_to_, &DatePicker::dateChanged, this, &VideoManagementPage::OnDateToChanged);
    top_bar->addWidget(date_to_);

    auto* reset_btn = new QPushButton(QStringLiteral("重置"), this);
    reset_btn_ = reset_btn;
    reset_btn->setCursor(Qt::PointingHandCursor);
    reset_btn->setStyleSheet(QString(
        "QPushButton {"
        "  background: transparent; color: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 12px;"
        "}"
        "QPushButton:hover { background: %3; }"
    ).arg(tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(reset_btn, &QPushButton::clicked, this, &VideoManagementPage::OnResetFilters);
    top_bar->addWidget(reset_btn);

    top_bar->addStretch();

    import_btn_ = new QPushButton(QStringLiteral("+ 导入视频"), this);
    import_btn_->setCursor(Qt::PointingHandCursor);
    import_btn_->setStyleSheet(QString(
        "QPushButton {"
        "  background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %3; }"
        "QPushButton:pressed { background: %4; }"
    ).arg(tm.Primary(), tm.White(), tm.PrimaryHover(), tm.PrimaryHover()));
    connect(import_btn_, &QPushButton::clicked, this, &VideoManagementPage::OnImportClicked);
    top_bar->addWidget(import_btn_);

    refresh_btn_ = new QPushButton(QStringLiteral("⟳ 刷新"), this);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(QString(
        "QPushButton {"
        "  background: transparent; color: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 16px;"
        "}"
        "QPushButton:hover { background: %3; }"
    ).arg(tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(refresh_btn_, &QPushButton::clicked, this, &VideoManagementPage::Refresh);
    top_bar->addWidget(refresh_btn_);

    root->addLayout(top_bar);

    // ── Table ────────────────────────────────────────────────

    model_ = new VideoTableModel(this);
    proxy_ = new VideoFilterProxy(this);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(Qt::DisplayRole);
    proxy_->setDynamicSortFilter(true);

    table_view_ = new QTableView(this);
    table_view_->setModel(proxy_);
    table_view_->setSortingEnabled(true);
    table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_view_->setAlternatingRowColors(true);
    table_view_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_view_->verticalHeader()->setVisible(false);
    table_view_->setShowGrid(false);

    auto* hdr = table_view_->horizontalHeader();
    hdr->setStretchLastSection(true);
    hdr->setSectionResizeMode(QHeaderView::Interactive);
    hdr->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hdr->setMinimumSectionSize(60);
    hdr->resizeSection(VideoTableModel::ColCheck,   30);
    hdr->resizeSection(VideoTableModel::ColName,           140);
    hdr->resizeSection(VideoTableModel::ColDescription,    120);
    hdr->resizeSection(VideoTableModel::ColNotes,          100);
    hdr->resizeSection(VideoTableModel::ColDuration,       70);
    hdr->resizeSection(VideoTableModel::ColResolution,     100);
    hdr->resizeSection(VideoTableModel::ColFrameRate,      60);
    hdr->resizeSection(VideoTableModel::ColRecordTime,     140);
    hdr->resizeSection(VideoTableModel::ColRecordLocation, 100);
    hdr->resizeSection(VideoTableModel::ColFileSize,       90);
    hdr->resizeSection(VideoTableModel::ColSavedPath,      200);

    // Header checkbox for select-all
    connect(hdr, &QHeaderView::sectionClicked, this, [this](int logicalIndex) {
        if (logicalIndex == VideoTableModel::ColCheck) {
            bool allChecked = !model_->CheckedIds().isEmpty()
                           && model_->CheckedIds().size() == static_cast<size_t>(model_->rowCount());
            model_->ToggleAll(!allChecked);
            UpdateBatchDeleteButton();
        }
    });

    table_view_->setStyleSheet(QString(
        "QTableView {"
        "  background: %1; border: 1px solid %2;"
        "  gridline-color: %3;"
        "  selection-background-color: %4;"
        "  selection-color: %5;"
        "}"
        "QTableView::item { padding: 6px 8px; }"
        "QTableView::item:alternate { background: %6; }"
        "QHeaderView::section {"
        "  background: %7; color: %8;"
        "  border: none; border-right: 1px solid %9;"
        "  border-bottom: 2px solid %10;"
        "  padding: 6px 8px; font-weight: bold;"
        "}"
    ).arg(tm.DeepBg(), tm.Border(), tm.SurfaceBg(), tm.SelectedBg(),
          tm.TextPrimary(), tm.AltRow(), tm.SurfaceBg(), tm.TextMuted(),
          tm.Border(), tm.Border()));

    connect(table_view_, &QTableView::customContextMenuRequested,
            this, &VideoManagementPage::OnContextMenu);
    connect(table_view_, &QTableView::doubleClicked,
            this, &VideoManagementPage::OnDoubleClicked);

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
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:disabled { background: %4; color: %5; }"
    ).arg(tm.Error(), tm.White(), tm.ErrorBg(), tm.HoverBg(), tm.TextDim()));
    connect(batch_delete_btn_, &QPushButton::clicked, this, &VideoManagementPage::OnBatchDelete);
    bottom_bar->addWidget(batch_delete_btn_);

    root->addLayout(bottom_bar);

    UpdateCountLabel();
}

void VideoManagementPage::LoadVideos() {
    auto records = repo_->FindAll();
    model_->SetRecords(std::move(records));
    UpdateCountLabel();
    UpdateBatchDeleteButton();
}

void VideoManagementPage::UpdateCountLabel() {
    int filtered = proxy_ ? proxy_->rowCount() : 0;
    count_label_->setText(QStringLiteral("共 %1 个视频").arg(filtered));
}

void VideoManagementPage::UpdateBatchDeleteButton() {
    int checked = model_->CheckedIds().size();
    batch_delete_btn_->setEnabled(checked > 0);
    batch_delete_btn_->setText(QStringLiteral("删除选中 (%1)").arg(checked));
}

void VideoManagementPage::Refresh() {
    LoadVideos();
}

void VideoManagementPage::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();
    search_box_->setStyleSheet(QString(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 8px; color: %3;"
        "}"
        "QLineEdit:focus { border-color: %4; }"
    ).arg(tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
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
        "  background: transparent; color: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 16px;"
        "}"
        "QPushButton:hover { background: %3; }"
    ).arg(tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    table_view_->setStyleSheet(QString(
        "QTableView {"
        "  background: %1; border: 1px solid %2;"
        "  gridline-color: %3;"
        "  selection-background-color: %4;"
        "  selection-color: %5;"
        "}"
        "QTableView::item { padding: 6px 8px; }"
        "QTableView::item:alternate { background: %6; }"
        "QHeaderView::section {"
        "  background: %7; color: %8;"
        "  border: none; border-right: 1px solid %9;"
        "  border-bottom: 2px solid %10;"
        "  padding: 6px 8px; font-weight: bold;"
        "}"
    ).arg(tm.DeepBg(), tm.Border(), tm.SurfaceBg(), tm.SelectedBg(),
          tm.TextPrimary(), tm.AltRow(), tm.SurfaceBg(), tm.TextMuted(),
          tm.Border(), tm.Border()));
    count_label_->setStyleSheet(QString("QLabel { color: %1; padding: 2px 0; }").arg(tm.TextDim()));
    batch_delete_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:disabled { background: %4; color: %5; }"
    ).arg(tm.Error(), tm.White(), tm.ErrorBg(), tm.HoverBg(), tm.TextDim()));
}

// ─── Slots ───────────────────────────────────────────────────

void VideoManagementPage::OnSearchTextChanged(const QString& text) {
    proxy_->SetNameFilter(text);
    UpdateCountLabel();
}

void VideoManagementPage::OnDateFromChanged(const QDate& date) {
    proxy_->SetDateFrom(date);
    UpdateCountLabel();
}

void VideoManagementPage::OnDateToChanged(const QDate& date) {
    proxy_->SetDateTo(date);
    UpdateCountLabel();
}

void VideoManagementPage::OnResetFilters() {
    search_box_->clear();

    date_from_->blockSignals(true);
    date_from_->setDate(QDate());
    date_from_->blockSignals(false);
    proxy_->SetDateFrom(QDate());

    date_to_->blockSignals(true);
    date_to_->setDate(QDate());
    date_to_->blockSignals(false);
    proxy_->SetDateTo(QDate());

    UpdateCountLabel();
}

void VideoManagementPage::OnImportClicked() {
    ImportVideoDialog dlg(repo_.get(), this);
    if (dlg.exec() == QDialog::Accepted)
        Refresh();
}

void VideoManagementPage::TriggerImport() {
    OnImportClicked();
}

int64_t VideoManagementPage::SelectedVideoId() const {
    auto indexes = table_view_->selectionModel()->selectedRows();
    if (indexes.isEmpty()) return 0;
    return indexes.first().data(Qt::UserRole).toLongLong();
}

void VideoManagementPage::OnDoubleClicked(const QModelIndex& index) {
    int64_t id = index.data(Qt::UserRole).toLongLong();
    if (id > 0)
        emit ViewDetailsRequested(id);
}

void VideoManagementPage::OnContextMenu(const QPoint& pos) {
    auto index = table_view_->indexAt(pos);
    if (!index.isValid()) return;

    table_view_->selectRow(index.row());
    int64_t vid = SelectedVideoId();
    if (vid == 0) return;

    auto& tm = ThemeManager::Instance();

    QMenu menu(this);
    menu.setStyleSheet(QString(
        "QMenu { background: %1; color: %2; border: 1px solid %3; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; }"
        "QMenu::item:selected { background: %4; }"
        "QMenu::separator { background: %5; height: 1px; margin: 4px 8px; }"
    ).arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.SelectedBg(), tm.Border()));

    auto* view_action = menu.addAction(QStringLiteral("查看详情"));
    auto* edit_action = menu.addAction(QStringLiteral("编辑信息"));
    menu.addSeparator();
    auto* delete_action = menu.addAction(QStringLiteral("删除"));

    QAction* chosen = menu.exec(table_view_->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == view_action) {
        emit ViewDetailsRequested(vid);
    } else if (chosen == edit_action) {
        emit EditRequested(vid);
    } else if (chosen == delete_action) {
        OnDeleteVideo();
    }
}

void VideoManagementPage::OnDeleteVideo() {
    int64_t vid = SelectedVideoId();
    if (vid == 0) return;

    auto rec = repo_->FindById(vid);
    if (!rec) return;

    QString name = QString::fromStdString(rec->name);

    auto result = QMessageBox::question(this,
        QStringLiteral("删除视频"),
        QStringLiteral("确定要删除视频 \"%1\" 吗？\n\n将同时删除记录和文件，此操作不可撤销。").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) return;

    if (!repo_->Delete(vid)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"),
            QStringLiteral("无法删除视频记录。"));
        return;
    }
    if (!rec->saved_path.empty()) {
        QFile::remove(QString::fromStdString(rec->saved_path));
    }
    Refresh();
    QMessageBox::information(this, QStringLiteral("删除成功"),
        QStringLiteral("视频 \"%1\" 已成功删除。").arg(name));
}

void VideoManagementPage::OnBatchDelete() {
    QSet<int64_t> ids = model_->CheckedIds();
    if (ids.isEmpty()) return;

    int count = ids.size();
    auto result = QMessageBox::question(this,
        QStringLiteral("批量删除"),
        QStringLiteral("确定要删除选中的 %1 个视频吗？\n\n将同时删除记录和文件，此操作不可撤销。").arg(count),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) return;

    int deleted = 0;
    for (int64_t id : ids) {
        auto rec = repo_->FindById(id);
        if (repo_->Delete(id)) {
            if (rec && !rec->saved_path.empty()) {
                QFile::remove(QString::fromStdString(rec->saved_path));
            }
            deleted++;
        }
    }
    Refresh();
    QMessageBox::information(this, QStringLiteral("删除完成"),
        QStringLiteral("成功删除 %1/%2 个视频。").arg(deleted).arg(count));
}

void VideoManagementPage::OnEditVideo() {
    int64_t vid = SelectedVideoId();
    if (vid > 0)
        emit EditRequested(vid);
}

void VideoManagementPage::OnViewDetails() {
    int64_t vid = SelectedVideoId();
    if (vid > 0)
        emit ViewDetailsRequested(vid);
}

void VideoManagementPage::OnSelectForProcessing() {
    int64_t vid = SelectedVideoId();
    if (vid == 0) return;
    auto rec = repo_->FindById(vid);
    if (rec) {
        emit VideoSelectedForProcessing(vid,
            QString::fromStdString(rec->name),
            QString::fromStdString(rec->saved_path));
    }
}

}  // namespace ui
}  // namespace omniface
