#pragma once

#include <QAbstractTableModel>
#include <QDate>
#include <QSet>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QWidget>

#include <memory>
#include <vector>

#include "omniface/db/video_repo.h"
#include "omniface/ui/widgets/date_picker.h"

class QTableView;
class QLineEdit;
class QLabel;
class QPushButton;

namespace omniface {
namespace db {
class VideoRepo;
struct VideoRecord;
}

namespace ui {

class VideoTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColCheck = 0,
        ColName,
        ColDescription,
        ColNotes,
        ColDuration,
        ColResolution,
        ColFrameRate,
        ColRecordTime,
        ColRecordLocation,
        ColFileSize,
        ColSavedPath,
        ColCreatedAt,
        ColCount
    };

    explicit VideoTableModel(QObject* parent = nullptr);

    void SetRecords(std::vector<db::VideoRecord> records);
    const db::VideoRecord* GetRecord(int row) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QSet<int64_t> CheckedIds() const;
    void ToggleAll(bool checked);

private:
    static QString FormatDuration(double seconds);
    static QString FormatFileSize(int64_t bytes);
    static QString FormatTimestamp(const std::string& iso);

    std::vector<db::VideoRecord> records_;
    QSet<int> checked_rows_;
};

class VideoFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit VideoFilterProxy(QObject* parent = nullptr);

    void SetNameFilter(const QString& text);
    void SetDateFrom(const QDate& date);
    void SetDateTo(const QDate& date);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString name_filter_;
    QDate date_from_;
    QDate date_to_;
    bool date_filter_active_ = false;
};

class VideoManagementPage : public QWidget {
    Q_OBJECT
public:
    explicit VideoManagementPage(std::shared_ptr<db::VideoRepo> repo, QWidget* parent = nullptr);

    void Refresh();
    void TriggerImport();
    void ReapplyTheme();
    void SetProcessingPage(class ProcessingPage* pp) { process_page_ = pp; }

protected:
    void showEvent(QShowEvent* event) override { Refresh(); QWidget::showEvent(event); }

signals:
    void VideoSelectedForProcessing(int64_t video_id, const QString& name, const QString& path);
    void ViewDetailsRequested(int64_t video_id);
    void EditRequested(int64_t video_id);
    void VideoDeleted(int64_t video_id);

private slots:
    void OnSearchTextChanged(const QString& text);
    void OnDateFromChanged(const QDate& date);
    void OnDateToChanged(const QDate& date);
    void OnImportClicked();
    void OnContextMenu(const QPoint& pos);
    void OnDeleteVideo();
    void OnBatchDelete();
    void OnEditVideo();
    void OnViewDetails();
    void OnSelectForProcessing();
    void OnDoubleClicked(const QModelIndex& index);
    void OnResetFilters();

private:
    void SetupUI();
    void LoadVideos();
    void UpdateCountLabel();
    void UpdateBatchDeleteButton();
    int64_t SelectedVideoId() const;

    std::shared_ptr<db::VideoRepo> repo_;
    class ProcessingPage* process_page_ = nullptr;
    QTableView* table_view_ = nullptr;
    QLineEdit* search_box_ = nullptr;
    DatePicker* date_from_ = nullptr;
    DatePicker* date_to_ = nullptr;
    QLabel* count_label_ = nullptr;
    QPushButton* import_btn_ = nullptr;
    QPushButton* reset_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* batch_delete_btn_ = nullptr;
    VideoTableModel* model_ = nullptr;
    VideoFilterProxy* proxy_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
