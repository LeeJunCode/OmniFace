#pragma once

#include <memory>
#include <string>

#include <QMainWindow>
#include <QStackedWidget>

#include "omniface/pipeline/config.h"
#include "omniface/ui/detail_page.h"
#include "omniface/ui/history_page.h"
#include "omniface/ui/process_page.h"
#include "omniface/ui/saved_media_page.h"

namespace omniface::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const PipelineConfig& config,
                        const std::string& config_path, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onViewDetails();
    void onTaskHistory();
    void onBackToProcess();
    void onDeleteTask(const std::string& dir_path);
    void onSettings();

private:
    void setupUI();
    void setupMenus();

    PipelineConfig config_;
    std::string config_path_;

    QStackedWidget* central_stack_ = nullptr;
    ProcessPage* process_page_ = nullptr;
    DetailPage* detail_page_ = nullptr;
    HistoryPage* history_page_ = nullptr;
    SavedMediaPage* saved_videos_page_ = nullptr;
    SavedMediaPage* saved_images_page_ = nullptr;

    QAction* saved_videos_action_ = nullptr;
    QAction* saved_images_action_ = nullptr;
};

}  // namespace omniface::ui
