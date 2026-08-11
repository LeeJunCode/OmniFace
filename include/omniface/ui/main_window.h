#pragma once

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <memory>
#include <QIcon>

namespace omniface {
namespace db {
class Database;
class VideoRepo;
class ImageRepo;
class TaskRepo;
}

namespace ui {

class HomePage;
class VideoManagementPage;
class ImageManagementPage;
class ProcessingPage;
class TaskHistoryPage;
class VideoDetailPage;
class ImageDetailPage;
class TaskDetailPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    void StatusChanged(const QString& status);
    void ProcessingStarted();
    void ProcessingStopped();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnPageChanged(int index);
    void OnBackFromDetail();
    void OnViewCsv(int64_t task_id);
    void OnViewCleanCsv(int64_t task_id);

private:
    void SetupMenuBar();
    void SetupSidebar();
    void SetupContentArea();
    void SetupStatusBar();
    void ApplyTheme();
    void ReapplyWidgetStyles();
    void ShowDetailPage(QWidget* page);

    QPushButton* CreateSidebarButton(const QIcon& icon, const QString& tooltip);

    std::unique_ptr<db::Database> database_;

    // Sidebar
    QWidget* sidebar_;
    QVBoxLayout* sidebar_layout_;
    QButtonGroup* nav_buttons_;
    QWidget* status_indicator_;
    QFrame* separator_ = nullptr;

    // Content
    QStackedWidget* content_stack_;

    // Main pages (indices 0-4)
    HomePage* home_page_;
    ProcessingPage* process_page_;
    VideoManagementPage* video_page_;
    ImageManagementPage* image_page_;
    TaskHistoryPage* history_page_;

    // Detail pages (indices 5-7)
    VideoDetailPage* video_detail_page_;
    ImageDetailPage* image_detail_page_;
    TaskDetailPage* task_detail_page_;

    int previous_page_index_ = 0;
    int64_t last_completed_task_id_ = 0;
    int64_t detail_video_id_ = 0;
    int64_t detail_image_id_ = 0;
    int64_t detail_task_id_ = 0;

    // Status bar
    QLabel* status_label_;
    QLabel* frame_label_;
    QLabel* fps_label_;
};

}  // namespace ui
}  // namespace omniface
