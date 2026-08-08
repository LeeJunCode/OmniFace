#include "omniface/ui/dialogs/image_selection_dialog.h"
#include "omniface/ui/theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "omniface/db/image_repo.h"
#include "omniface/ui/pages/image_management_page.h"

namespace omniface {
namespace ui {

ImageSelectionDialog::ImageSelectionDialog(std::shared_ptr<db::ImageRepo> repo, QWidget* parent)
    : QDialog(parent), repo_(std::move(repo)) {
    setWindowTitle(QStringLiteral("从数据库选择图片"));
    setMinimumSize(750, 480);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; color: %2; }").arg(ThemeManager::Instance().SurfaceBg(), ThemeManager::Instance().TextPrimary()));
    SetupUI();
    LoadImages();
}

void ImageSelectionDialog::SetupUI() {
    auto& tm = ThemeManager::Instance();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // ── Search bar ──────────────────────────────────────────

    auto* top_bar = new QHBoxLayout();
    top_bar->setSpacing(8);

    search_box_ = new QLineEdit(this);
    search_box_->setPlaceholderText(QStringLiteral("搜索图片名称..."));
    search_box_->setClearButtonEnabled(true);
    search_box_->setFixedWidth(220);
    search_box_->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 8px; color: %3;"
        "}"
        "QLineEdit:focus { border-color: %4; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    connect(search_box_, &QLineEdit::textChanged, this, &ImageSelectionDialog::OnSearchTextChanged);
    top_bar->addWidget(search_box_);

    top_bar->addStretch();
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
    table_view_->setAlternatingRowColors(true);
    table_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_view_->verticalHeader()->setVisible(false);
    table_view_->setShowGrid(false);
    table_view_->setColumnHidden(ImageTableModel::ColCheck, true);

    auto* hdr = table_view_->horizontalHeader();
    hdr->setStretchLastSection(true);
    hdr->setSectionResizeMode(QHeaderView::Interactive);
    hdr->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hdr->setMinimumSectionSize(60);
    hdr->resizeSection(ImageTableModel::ColName,        140);
    hdr->resizeSection(ImageTableModel::ColDescription, 120);
    hdr->resizeSection(ImageTableModel::ColNotes,       100);
    hdr->resizeSection(ImageTableModel::ColHasFace,     80);
    hdr->resizeSection(ImageTableModel::ColConfidence,  80);
    hdr->resizeSection(ImageTableModel::ColSavedPath,   200);

    table_view_->setStyleSheet(QStringLiteral(
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
        "}").arg(tm.DeepBg(), tm.Border(), tm.SurfaceBg(), tm.SelectedBg(),
                tm.TextPrimary(), tm.AltRow(), tm.SurfaceBg(), tm.TextMuted(),
                tm.Border(), tm.Border()));

    connect(table_view_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImageSelectionDialog::OnSelectionChanged);
    connect(table_view_, &QTableView::doubleClicked,
            this, &ImageSelectionDialog::OnDoubleClicked);

    root->addWidget(table_view_, 1);

    // ── Bottom bar ───────────────────────────────────────────

    auto* bottom = new QHBoxLayout();
    bottom->addStretch();

    select_btn_ = new QPushButton(QStringLiteral("选择"), this);
    select_btn_->setEnabled(false);
    select_btn_->setCursor(Qt::PointingHandCursor);
    select_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 8px 24px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:disabled { background: %4; color: %5; }").arg(tm.Primary(), tm.White(), tm.PrimaryHover(), tm.HoverBg(), tm.TextDim()));
    connect(select_btn_, &QPushButton::clicked, this, &ImageSelectionDialog::OnSelectClicked);
    bottom->addWidget(select_btn_);

    auto* cancel_btn = new QPushButton(QStringLiteral("取消"), this);
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 8px 24px; }"
        "QPushButton:hover { background: %3; }").arg("transparent", tm.TextPrimary(), tm.HoverBg()));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    bottom->addWidget(cancel_btn);

    root->addLayout(bottom);
}

void ImageSelectionDialog::LoadImages() {
    auto records = repo_->FindAll();
    model_->SetRecords(std::move(records));
}

void ImageSelectionDialog::OnSearchTextChanged(const QString& text) {
    proxy_->SetNameFilter(text);
}

void ImageSelectionDialog::OnSelectionChanged() {
    auto indexes = table_view_->selectionModel()->selectedRows();
    select_btn_->setEnabled(!indexes.isEmpty());
}

void ImageSelectionDialog::OnDoubleClicked(const QModelIndex& index) {
    auto srcIndex = proxy_->mapToSource(index);
    const auto* rec = model_->GetRecord(srcIndex.row());
    if (!rec) return;

    auto result = QMessageBox::question(this,
        QStringLiteral("确认选择"),
        QStringLiteral("确定选择图片 \"%1\" 吗？").arg(QString::fromStdString(rec->name)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes) {
        selected_id_ = rec->id;
        selected_name_ = QString::fromStdString(rec->name);
        selected_embedding_.features = rec->embedding;
        selected_embedding_.valid = true;
        accept();
    }
}

void ImageSelectionDialog::OnSelectClicked() {
    auto indexes = table_view_->selectionModel()->selectedRows();
    if (indexes.isEmpty()) return;

    auto srcIndex = proxy_->mapToSource(indexes.first());
    const auto* rec = model_->GetRecord(srcIndex.row());
    if (!rec) return;

    auto result = QMessageBox::question(this,
        QStringLiteral("确认选择"),
        QStringLiteral("确定选择图片 \"%1\" 吗？").arg(QString::fromStdString(rec->name)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes) {
        selected_id_ = rec->id;
        selected_name_ = QString::fromStdString(rec->name);
        selected_embedding_.features = rec->embedding;
        selected_embedding_.valid = true;
        accept();
    }
}

}  // namespace ui
}  // namespace omniface
