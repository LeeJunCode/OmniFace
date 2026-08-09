#pragma once

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QWidget>

#include <memory>

namespace omniface {
namespace db {
struct TaskRecord;
class TaskRepo;
class VideoRepo;
class ImageRepo;
}

namespace ui {

class VideoPlayerWidget;
class BaseDetailWidget;
class QualityReportWidget;

class TaskDetailPage : public QWidget {
    Q_OBJECT
public:
    explicit TaskDetailPage(std::shared_ptr<db::TaskRepo> task_repo,
                            std::shared_ptr<db::VideoRepo> video_repo,
                            std::shared_ptr<db::ImageRepo> image_repo,
                            QWidget* parent = nullptr);

    void LoadTask(int64_t task_id);
    void ReapplyTheme();

signals:
    void BackRequested();
    void EditRequested(int64_t task_id);
    void ViewCsvRequested(int64_t task_id);
    void ViewCleanCsvRequested(int64_t task_id);

private slots:
    void OnOutputVideoPositionChanged(qint64 ms);
    void OnSourceVideoPositionChanged(qint64 ms);
    void OnShowRawInfo();
    void OnShowAnalysis();
    void OnShowQuality();
    void OnShowCleanQuality();
    void OnCleanData();

private:
    void SetupUI();
    void PopulateRawInfo(const db::TaskRecord& task);
    void CreateBackendWidget(const std::string& backend);
    void RunQualityEvaluation(const QString& main_csv, const QString& quality_csv,
                              QualityReportWidget* target);
    void SwitchToQualityPanel();
    void SwitchToCleanQualityPanel();

    std::shared_ptr<db::TaskRepo> task_repo_;
    std::shared_ptr<db::VideoRepo> video_repo_;
    std::shared_ptr<db::ImageRepo> image_repo_;
    int64_t task_id_ = 0;
    double output_video_fps_ = 24.0;

    // Left: output video
    VideoPlayerWidget* output_player_ = nullptr;
    QLabel* output_frame_label_ = nullptr;

    // Right: mode buttons
    QPushButton* raw_btn_ = nullptr;
    QPushButton* analysis_btn_ = nullptr;
    QPushButton* quality_btn_ = nullptr;
    QPushButton* clean_eval_btn_ = nullptr;
    QPushButton* clean_btn_ = nullptr;
    QWidget* raw_info_panel_ = nullptr;
    QScrollArea* raw_scroll_area_ = nullptr;
    QWidget* analysis_panel_ = nullptr;
    QScrollArea* quality_scroll_area_ = nullptr;
    QScrollArea* clean_quality_scroll_area_ = nullptr;

    // Raw info panel widgets
    VideoPlayerWidget* source_player_ = nullptr;
    QLabel* source_frame_label_ = nullptr;
    QLabel* source_image_label_ = nullptr;
    QLabel* raw_video_name_ = nullptr;
    QLabel* raw_video_duration_ = nullptr;
    QLabel* raw_video_resolution_ = nullptr;
    QLabel* raw_video_fps_ = nullptr;
    QLabel* raw_video_location_ = nullptr;
    QLabel* raw_image_name_ = nullptr;
    QLabel* raw_image_confidence_ = nullptr;
    QLabel* raw_bbox_ = nullptr;
    QLabel* raw_task_backend_ = nullptr;
    QLabel* raw_task_frames_ = nullptr;
    QLabel* raw_task_fps_ = nullptr;
    QLabel* raw_task_time_ = nullptr;
    QLabel* raw_task_status_ = nullptr;
    QPushButton* csv_btn_ = nullptr;
    QPushButton* clean_csv_btn_ = nullptr;

    QPushButton* back_btn_ = nullptr;
    QPushButton* edit_btn_ = nullptr;
    QLabel* title_label_ = nullptr;
    QSplitter* splitter_ = nullptr;

    BaseDetailWidget* detail_widget_ = nullptr;
    QualityReportWidget* quality_widget_ = nullptr;
    QualityReportWidget* clean_quality_widget_ = nullptr;
    bool quality_evaluated_ = false;
    bool clean_quality_evaluated_ = false;
};

}  // namespace ui
}  // namespace omniface
