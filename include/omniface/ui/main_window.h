#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <opencv2/videoio.hpp>

#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "omniface/pipeline/config.h"
#include "omniface/pipeline/pipeline.h"
#include "omniface/types.h"
#include "omniface/ui/processing_dashboard.h"
#include "omniface/ui/video_player_widget.h"




namespace omniface::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const PipelineConfig& config, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onOpenVideo();
    void onOpenReference();
    void onPlay();
    void onPause();
    void onStop();
    void onStep();
    void onReplay();
    void onRecognizeOnly();
    void onOpenSavedVideo();
    void onOpenSavedImage();
    void processNextFrame();
    void onViewDetails();
    void onBackToProcess();
    void onTaskHistory();
    void onDeleteTask(const std::string& dir_path);

private:
    void setupUI();
    void setupStyleSheet();
    void updateRefThumbnails();
    void updateStatsDisplay(double fps, int faceCount, bool targetFound);
    void setUiEnabled(bool running);
    void setFinishedState();
    QImage matToQImage(const cv::Mat& bgr);

    void showDetailPage(const std::string& output_dir);
    void showHistoryPage();
    void showSavedVideosPage();
    void showSavedImagesPage();
    void onDetailFrameSync(qint64 positionMs);
    void onDeleteSavedFile(const std::string& file_path);

    QStackedWidget* central_stack_ = nullptr;
    QWidget* process_page_ = nullptr;
    QWidget* detail_page_ = nullptr;
    QWidget* history_page_ = nullptr;
    QWidget* saved_videos_page_ = nullptr;
    QWidget* saved_images_page_ = nullptr;

    QPushButton* open_video_btn_ = nullptr;
    QPushButton* open_saved_video_btn_ = nullptr;
    QPushButton* open_ref_btn_ = nullptr;
    QPushButton* open_saved_ref_btn_ = nullptr;
    VideoPlayerWidget* video_preview_ = nullptr;
    QLabel* ref_original_thumbnail_ = nullptr;
    QLabel* ref_detected_thumbnail_ = nullptr;

    QPushButton* play_btn_ = nullptr;
    QPushButton* pause_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QPushButton* step_btn_ = nullptr;
    QPushButton* replay_btn_ = nullptr;
    QPushButton* recognize_only_btn_ = nullptr;

    QLabel* fps_display_ = nullptr;
    QLabel* face_count_display_ = nullptr;
    QLabel* target_status_display_ = nullptr;
    QLabel* counter_summary_ = nullptr;

    QLabel* video_label_ = nullptr;
    QProgressBar* processing_progress_ = nullptr;
    FpsPanel* fps_panel_ = nullptr;

    QWidget* completion_panel_ = nullptr;
    QLabel* completion_label_ = nullptr;
    QPushButton* view_details_btn_ = nullptr;

    VideoPlayerWidget* detail_video_ = nullptr;
    VideoPlayerWidget* detail_orig_video_ = nullptr;
    QLabel* detail_ref_thumb_ = nullptr;
    QLabel* detail_ref_detected_ = nullptr;
    QLabel* detail_info_label_ = nullptr;
    QLabel* detail_face_thumb_ = nullptr;
    QLabel* detail_data_label_ = nullptr;

    std::vector<std::string> detail_csv_headers_;
    std::vector<std::vector<std::string>> detail_csv_rows_;
    std::unordered_map<int, int> detail_frame_to_row_;
    std::vector<std::string> detail_aligned_paths_;
    double detail_tracked_fps_ = 0.0;
    bool detail_from_history_ = false;

    QScrollArea* history_scroll_ = nullptr;
    QWidget* history_content_ = nullptr;

    QScrollArea* saved_videos_scroll_ = nullptr;
    QWidget* saved_videos_content_ = nullptr;
    QScrollArea* saved_images_scroll_ = nullptr;
    QWidget* saved_images_content_ = nullptr;
    VideoPlayerWidget* saved_video_preview_ = nullptr;

    QLabel* status_label_ = nullptr;

    PipelineConfig config_;
    std::unique_ptr<Pipeline> pipeline_;

    std::string video_path_;
    std::string reference_path_;
    bool is_running_ = false;
    bool is_paused_ = false;
    bool step_requested_ = false;
    int frame_count_ = 0;
    int total_frame_count_ = 0;
    double video_fps_ = 0.0;
    bool has_finished_once_ = false;
    bool recognition_only_ = false;

    FaceEmbedding cached_reference_embedding_;
    bool reference_cached_ = false;

    QImage qt_image_cache_;

    std::string last_output_dir_;
    std::string current_detail_dir_;

    std::chrono::steady_clock::time_point last_fps_time_;
    int fps_frame_count_ = 0;
    double current_fps_ = 0.0;

    std::chrono::steady_clock::time_point last_dashboard_time_;

    int64_t total_faces_detected_ = 0;
    int64_t total_matches_ = 0;
    int64_t total_openface_analyses_ = 0;
    std::chrono::steady_clock::time_point run_start_time_;
};

}  // namespace omniface::ui
