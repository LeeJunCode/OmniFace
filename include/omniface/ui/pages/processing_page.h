#pragma once

#include <QElapsedTimer>
#include <QGroupBox>
#include <QLabel>
#include <QMutex>
#include <QProgressBar>
#include <QScrollArea>
#include <QWidget>

#include <atomic>
#include <memory>
#include <thread>

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>

#include "omniface/pipeline/framework/config.h"
#include "omniface/types.h"

class QLabel;
class QPushButton;
class QGroupBox;
class QTimer;
class QAction;

namespace omniface {

class BasePipeline;

namespace db {
class VideoRepo;
class ImageRepo;
}

namespace ui {

class VideoPlayerWidget;

class FrameDisplayWidget : public QWidget {
    Q_OBJECT
public:
    explicit FrameDisplayWidget(QWidget* parent = nullptr);
    void ReapplyTheme();

public slots:
    void SetFrame(const cv::Mat& bgr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap current_frame_;
    QMutex frame_mutex_;
};

class ProcessWorker : public QObject {
    Q_OBJECT
public:
    explicit ProcessWorker(QObject* parent = nullptr);
    ~ProcessWorker() override;

    void SetVideoPath(const QString& path);
    void SetReference(const FaceEmbedding& embedding);
    void SetConfig(const PipelineConfig& config);

    void Start();
    void Pause();
    void Resume();
    void Step();
    void Stop();
    std::string GetOutputDir() const;
    std::string GetOutputVideoPath() const;

signals:
    void FrameProcessed(cv::Mat bgr, int frameIndex, int totalFrames,
                        double pipelineFps, double detRecFps, double analysisFps,
                        int faceCount, int matchCount, int analysisSuccessCount,
                        float currentSimilarity, float peakSimilarity);
    void ProcessingStarted();
    void ProcessingPaused();
    void ProcessingStopped();
    void ProcessingFinished();
    void ProcessingError(const QString& message);

private:
    void ProcessLoop();

    std::unique_ptr<BasePipeline> pipeline_;
    std::unique_ptr<std::thread> thread_;
    QString video_path_;
    FaceEmbedding reference_;
    PipelineConfig config_;

    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> step_requested_{false};

    QElapsedTimer elapsed_;
    int total_frames_ = 0;
    int processed_frames_ = 0;
    int face_count_total_ = 0;
    int match_count_total_ = 0;
    int analysis_success_count_ = 0;
    float latest_similarity_ = 0.0f;
    float peak_similarity_ = 0.0f;
    double det_rec_latency_sum_ = 0.0;
    double analysis_latency_sum_ = 0.0;
};

class ProcessingPage : public QWidget {
    Q_OBJECT
public:
    explicit ProcessingPage(std::shared_ptr<db::VideoRepo> video_repo,
                            std::shared_ptr<db::ImageRepo> image_repo,
                            QWidget* parent = nullptr);
    ~ProcessingPage() override;

public slots:
    void SetSelectedVideo(int64_t video_id, const QString& name, const QString& path);
    void SetSelectedImage(int64_t image_id, const QString& name, const FaceEmbedding& embedding);
    void SetConfig(const PipelineConfig& config);

    bool IsProcessing() const { return state_ == State::Running || state_ == State::Paused; }
    const PipelineConfig& GetConfig() const { return config_; }
    std::string GetWorkerOutputDir() const;
    std::string GetWorkerOutputVideoPath() const;

    void ReleaseMediaResources();
    bool IsVideoLocked(int64_t video_id) const;
    bool IsImageLocked(int64_t image_id) const;

public slots:
    void OnStop();
    void ReapplyTheme();

signals:
    void ProcessingActive(bool active);
    void ProcessingCompleted(int64_t video_id, int64_t image_id,
                             QString backend, double avg_fps, int total_frames,
                             QString output_dir, QString status);
    void ViewTaskResultRequested();

private slots:
    void OnStart();
    void OnPause();
    void OnResume();
    void OnStep();
    void OnFrameProcessed(cv::Mat bgr, int frameIndex, int totalFrames,
                          double pipelineFps, double detRecFps, double analysisFps,
                          int faceCount, int matchCount, int analysisSuccessCount,
                          float currentSimilarity, float peakSimilarity);
    void OnWorkerStarted();
    void OnWorkerPaused();
    void OnWorkerStopped();
    void OnWorkerFinished();
    void UpdateElapsedTime();

    void OnVideoFromFile();
    void OnVideoFromDatabase();
    void OnImageFromFile();
    void OnImageFromDatabase();
    void OnBackendSelected(QAction* action);

private:
    void SetupUI();
    void UpdateButtonStates();
    bool CheckReady();
    QWidget* CreateMediaPanel();
    QGroupBox* CreateStatsPanel();
    void UpdateMediaPanel();
    void UpdateAnalysisFpsLabel();
    QString AnalysisBackendDisplayName() const;

    int64_t AutoImportVideo(const QString& sourcePath);
    int64_t AutoImportImage(const QString& sourcePath);

    // Selection state
    int64_t selected_video_id_ = 0;
    int64_t selected_image_id_ = 0;
    QString selected_video_name_;
    QString selected_video_path_;
    QString selected_image_name_;
    QString selected_image_path_;
    FaceEmbedding selected_embedding_;
    PipelineConfig config_;

    // Repos
    std::shared_ptr<db::VideoRepo> video_repo_;
    std::shared_ptr<db::ImageRepo> image_repo_;

    // Worker
    ProcessWorker* worker_ = nullptr;

    // UI - top bar selection buttons
    QPushButton* video_btn_ = nullptr;
    QPushButton* image_btn_ = nullptr;
    QPushButton* backend_btn_ = nullptr;

    // UI - media panel (left)
    QWidget* media_panel_ = nullptr;
    QGroupBox* video_grp_ = nullptr;
    QGroupBox* image_grp_ = nullptr;
    QGroupBox* stats_panel_ = nullptr;
    VideoPlayerWidget* media_video_player_ = nullptr;
    QLabel* media_image_label_ = nullptr;
    QLabel* media_video_placeholder_ = nullptr;
    QLabel* media_image_placeholder_ = nullptr;

    // UI - media info labels (video)
    QLabel* video_info_name_ = nullptr;
    QLabel* video_info_duration_ = nullptr;
    QLabel* video_info_resolution_ = nullptr;
    QLabel* video_info_fps_ = nullptr;
    QLabel* video_info_size_ = nullptr;

    // UI - media info labels (image)
    QLabel* image_info_name_ = nullptr;
    QLabel* image_info_confidence_ = nullptr;
    QLabel* image_info_dimensions_ = nullptr;

    // UI - display (center)
    FrameDisplayWidget* display_ = nullptr;
    QProgressBar* progress_bar_ = nullptr;

    // UI - stats (right)
    QLabel* pipeline_fps_label_ = nullptr;
    QLabel* det_rec_fps_label_ = nullptr;
    QLabel* analysis_fps_title_label_ = nullptr;
    QLabel* analysis_fps_label_ = nullptr;
    QLabel* processed_frames_label_ = nullptr;
    QLabel* face_count_label_ = nullptr;
    QLabel* match_count_label_ = nullptr;
    QLabel* analysis_success_label_ = nullptr;
    QLabel* current_similarity_label_ = nullptr;
    QLabel* peak_similarity_label_ = nullptr;
    QLabel* elapsed_time_label_ = nullptr;
    // UI - buttons
    QPushButton* start_btn_ = nullptr;
    QPushButton* view_result_btn_ = nullptr;
    QPushButton* pause_btn_ = nullptr;
    QPushButton* resume_btn_ = nullptr;
    QPushButton* step_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;

    // UI - scroll area
    QScrollArea* left_scroll_area_ = nullptr;

    // State
    enum class State { Idle, Running, Paused, Finished };
    State state_ = State::Idle;

    // Elapsed timer
    QTimer* elapsed_timer_ = nullptr;
    int elapsed_seconds_ = 0;
    int total_frame_count_ = 0;
    int last_processed_frame_ = 0;
    bool was_processing_ = false;
};

}  // namespace ui
}  // namespace omniface
