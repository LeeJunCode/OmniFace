#pragma once

#include <QComboBox>
#include <QLabel>
#include <QSet>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QWidget>

#include "omniface/ui/widgets/date_picker.h"

#include <memory>
#include <string>

#include "omniface/db/task_repo.h"

class QTableView;
class QPushButton;
class QLineEdit;

namespace omniface {
namespace db {
class VideoRepo;
class ImageRepo;
}

namespace ui {

struct TaskDisplayInfo {
    db::TaskRecord task;
    QString video_name;
    QString image_name;
};

class TaskTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColCheck = 0,
        ColName,
        ColDate,
        ColVideoName,
        ColImageName,
        ColBackend,
        ColOutputTypes,
        ColStatus,
        ColFrames,
        ColAvgFps,
        ColDescription,
        ColNotes,
        ColCount
    };

    explicit TaskTableModel(QObject* parent = nullptr);

    void SetRecords(std::vector<TaskDisplayInfo> records);
    const TaskDisplayInfo* GetRecord(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QSet<int64_t> CheckedIds() const;
    void ToggleAll(bool checked);

private:
    static QString FormatDate(const std::string& iso);
    static QString StatusText(const std::string& status);
    static QString FormatFrameCount(int frames);

    std::vector<TaskDisplayInfo> records_;
    QSet<int> checked_rows_;
};

class TaskFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit TaskFilterProxy(QObject* parent = nullptr);

    void SetBackendFilter(const QString& backend);
    void SetStatusFilter(const QString& status);
    void SetDateFrom(const QDate& date);
    void SetDateTo(const QDate& date);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString backend_filter_;
    QString status_filter_;
    QDate date_from_;
    QDate date_to_;
};

class TaskHistoryPage : public QWidget {
    Q_OBJECT
public:
    explicit TaskHistoryPage(std::shared_ptr<db::TaskRepo> task_repo,
                             std::shared_ptr<db::VideoRepo> video_repo,
                             std::shared_ptr<db::ImageRepo> image_repo,
                             QWidget* parent = nullptr);

    void Refresh();
    void ReapplyTheme();

protected:
    void showEvent(QShowEvent* event) override { Refresh(); QWidget::showEvent(event); }

signals:
    void ViewTaskDetailRequested(int64_t task_id);
    void ViewCsvRequested(int64_t task_id);
    void EditRequested(int64_t task_id);

private slots:
    void OnBackendFilterChanged(int index);
    void OnStatusFilterChanged(int index);
    void OnDateFromChanged(const QDate& date);
    void OnDateToChanged(const QDate& date);
    void OnContextMenu(const QPoint& pos);
    void OnDoubleClicked(const QModelIndex& index);
    void OnDeleteTask();
    void OnBatchDelete();
    void OnResetFilters();

private:
    void SetupUI();
    void LoadTasks();
    void UpdateCountLabel();
    void UpdateBatchDeleteButton();
    int64_t SelectedTaskId() const;

    std::shared_ptr<db::TaskRepo> task_repo_;
    std::shared_ptr<db::VideoRepo> video_repo_;
    std::shared_ptr<db::ImageRepo> image_repo_;

    TaskTableModel* model_ = nullptr;
    TaskFilterProxy* proxy_ = nullptr;

    QTableView* table_view_ = nullptr;
    QComboBox* backend_combo_ = nullptr;
    QComboBox* status_combo_ = nullptr;
    DatePicker* date_from_ = nullptr;
    DatePicker* date_to_ = nullptr;
    QLabel* count_label_ = nullptr;
    QPushButton* reset_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* batch_delete_btn_ = nullptr;
};

}  // namespace ui
}  // namespace omniface
