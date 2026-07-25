#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

#include <opencv2/videoio.hpp>

#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include "omniface/pipeline/config.h"
#include "omniface/pipeline/pipeline.h"
#include "omniface/types.h"
#include "omniface/ui/video_preview_widget.h"
#include "omniface/ui/au_panel.h"

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
    void onMatchThresholdChanged(int value);
    void onDetectConfidenceChanged(int value);

private:
    void setupUI();
    void setupStyleSheet();
    void generateVideoThumbnail();
    void updateRefThumbnails();
    void updateStatsDisplay(double fps, int faceCount, bool targetFound);
    void updateStateDisplay();
    void setUiEnabled(bool running);
    void setFinishedState();
    QImage matToQImage(const cv::Mat& bgr);

    // File selection
    QPushButton* open_video_btn_ = nullptr;
    QPushButton* open_ref_btn_ = nullptr;
    VideoPreviewWidget* video_preview_ = nullptr;
    QLabel* ref_original_thumbnail_ = nullptr;
    QLabel* ref_detected_thumbnail_ = nullptr;

    // Playback controls
    QPushButton* play_btn_ = nullptr;
    QPushButton* pause_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QPushButton* step_btn_ = nullptr;
    QPushButton* replay_btn_ = nullptr;

    // Progress
    QSlider* progress_slider_ = nullptr;
    QLabel* time_display_ = nullptr;

    // Thresholds
    QSlider* match_threshold_slider_ = nullptr;
    QSlider* detect_confidence_slider_ = nullptr;
    QLabel* match_value_label_ = nullptr;
    QLabel* detect_value_label_ = nullptr;

    // Statistics
    QLabel* fps_display_ = nullptr;
    QLabel* face_count_display_ = nullptr;
    QLabel* target_status_display_ = nullptr;

    // Video display
    QLabel* video_label_ = nullptr;

    // AU panel
    AUPanel* au_panel_ = nullptr;

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

    // FPS tracking
    std::chrono::steady_clock::time_point last_fps_time_;
    int fps_frame_count_ = 0;
    double current_fps_ = 0.0;
};

}  // namespace omniface::ui
