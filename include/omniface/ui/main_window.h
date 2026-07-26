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
    void processNextFrame();

private:
    void setupUI();
    void setupStyleSheet();
    void updateRefThumbnails();
    void updateStatsDisplay(double fps, int faceCount, bool targetFound);
    void setUiEnabled(bool running);
    void setFinishedState();
    QImage matToQImage(const cv::Mat& bgr);

    // File selection content
    QPushButton* open_video_btn_ = nullptr;
    QPushButton* open_ref_btn_ = nullptr;
    VideoPlayerWidget* video_preview_ = nullptr;
    QLabel* ref_original_thumbnail_ = nullptr;
    QLabel* ref_detected_thumbnail_ = nullptr;

    // Run controls content
    QPushButton* play_btn_ = nullptr;
    QPushButton* pause_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QPushButton* step_btn_ = nullptr;
    QPushButton* replay_btn_ = nullptr;

    // Real-time stats
    QLabel* fps_display_ = nullptr;
    QLabel* face_count_display_ = nullptr;
    QLabel* target_status_display_ = nullptr;
    QLabel* counter_summary_ = nullptr;

    // Center: video display
    QLabel* video_label_ = nullptr;
    QProgressBar* processing_progress_ = nullptr;
    FpsPanel* fps_panel_ = nullptr;

    // Status bar
    QLabel* status_label_ = nullptr;

    // Pipeline
    PipelineConfig config_;
    std::unique_ptr<Pipeline> pipeline_;

    // State
    std::string video_path_;
    std::string reference_path_;
    bool is_running_ = false;
    bool is_paused_ = false;
    bool step_requested_ = false;
    int frame_count_ = 0;
    int total_frame_count_ = 0;
    double video_fps_ = 0.0;
    bool has_finished_once_ = false;

    // Cached reference embedding
    FaceEmbedding cached_reference_embedding_;
    bool reference_cached_ = false;

    // Cached QImage buffer for frame display
    QImage qt_image_cache_;

    // FPS tracking (overall)
    std::chrono::steady_clock::time_point last_fps_time_;
    int fps_frame_count_ = 0;
    double current_fps_ = 0.0;

    // Dashboard update timing
    std::chrono::steady_clock::time_point last_dashboard_time_;

    // Cumulative counters
    int64_t total_faces_detected_ = 0;
    int64_t total_matches_ = 0;
    int64_t total_openface_analyses_ = 0;
    std::chrono::steady_clock::time_point run_start_time_;
};

}  // namespace omniface::ui
