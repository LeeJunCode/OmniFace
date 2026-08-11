#pragma once

#include <QAbstractTableModel>
#include <QSet>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QWidget>

#include <memory>
#include <vector>

#include "omniface/db/image_repo.h"

class QTableView;
class QLineEdit;
class QLabel;
class QPushButton;

namespace omniface {
namespace ui {

class ImageTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColCheck = 0,
        ColName,
        ColDescription,
        ColNotes,
        ColConfidence,
        ColSavedPath,
        ColCreatedAt,
        ColCount
    };

    explicit ImageTableModel(QObject* parent = nullptr);

    void SetRecords(std::vector<db::ImageRecord> records);
    const db::ImageRecord* GetRecord(int row) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QSet<int64_t> CheckedIds() const;
    void ToggleAll(bool checked);

    static QString FormatTimestamp(const std::string& iso);

private:
    std::vector<db::ImageRecord> records_;
    QSet<int> checked_rows_;
};

class ImageFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit ImageFilterProxy(QObject* parent = nullptr);

    void SetNameFilter(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString name_filter_;
};

class ImageManagementPage : public QWidget {
    Q_OBJECT
public:
    explicit ImageManagementPage(std::shared_ptr<db::ImageRepo> repo, QWidget* parent = nullptr);

    void Refresh();
    void TriggerImport();
    void ReapplyTheme();
    void SetProcessingPage(class ProcessingPage* pp) { process_page_ = pp; }

protected:
    void showEvent(QShowEvent* event) override { Refresh(); QWidget::showEvent(event); }

signals:
    void ImageSelectedForProcessing(int64_t image_id, const QString& name);
    void ViewDetailsRequested(int64_t image_id);
    void EditRequested(int64_t image_id);
    void ImageDeleted(int64_t image_id);

private slots:
    void OnSearchTextChanged(const QString& text);
    void OnImportClicked();
    void OnContextMenu(const QPoint& pos);
    void OnDeleteImage();
    void OnBatchDelete();
    void OnEditImage();
    void OnViewDetails();
    void OnDoubleClicked(const QModelIndex& index);
    void OnResetFilters();

private:
    void SetupUI();
    void LoadImages();
    void UpdateCountLabel();
    void UpdateBatchDeleteButton();
    int64_t SelectedImageId() const;

    std::shared_ptr<db::ImageRepo> repo_;
    class ProcessingPage* process_page_ = nullptr;
    QTableView* table_view_ = nullptr;
    QLineEdit* search_box_ = nullptr;
    QLabel* count_label_ = nullptr;
    QPushButton* import_btn_ = nullptr;
    QPushButton* reset_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* batch_delete_btn_ = nullptr;
    ImageTableModel* model_ = nullptr;
    ImageFilterProxy* proxy_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
