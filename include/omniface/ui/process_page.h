#pragma once

#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "omniface/pipeline/config.h"
#include "omniface/pipeline/pipeline.h"
#include "omniface/ui/app_state.h"
#include "omniface/ui/au_panel.h"
#include "omniface/ui/onnx_analysis_panel.h"
#include "omniface/ui/processing_dashboard.h"
#include "omniface/ui/video_player_widget.h"

namespace omniface::ui {

class ProcessPage : public QWidget {
    Q_OBJECT

public:
    explicit ProcessPage(const PipelineConfig& config, QWidget* parent = nullptr);

    void setVideoPath(const std::string& path);
    void setReferencePath(const std::string& path);
    void updateRefThumbnails();
    void updateConfig(const PipelineConfig& config);

signals:
    void processingFinished(const std::string& output_dir);
    void statusMessage(const QString& msg);
    void navigateToDetail(const std::string& dir);
    void processingStateChanged(bool running);

public slots:
    void onOpenVideo();
    void onOpenReference();
    void onOpenSavedVideo();
    void onOpenSavedImage();
    void onPlay();
    void onPause();
    void onStop();
    void onStep();
    void onReplay();
    void onRecognizeOnly();

private slots:
    void processNextFrame();

private:
    void setupUI();
    void updateStatsDisplay(double fps, int faceCount, bool targetFound);
    void setUiEnabled(bool running);
    void setFinishedState();
    QImage matToQImage(const cv::Mat& bgr);

    PipelineConfig config_;
    AppState state_;

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
    OnnxAnalysisPanel* onnx_panel_ = nullptr;
    AUPanel* au_panel_ = nullptr;

    QWidget* completion_panel_ = nullptr;
    QLabel* completion_label_ = nullptr;
    QPushButton* view_details_btn_ = nullptr;

    std::unique_ptr<Pipeline> pipeline_;

    QImage qt_image_cache_;
};

}  // namespace omniface::ui
