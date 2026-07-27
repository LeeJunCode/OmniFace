#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <opencv2/core/mat.hpp>

#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

#include "omniface/ui/video_player_widget.h"

namespace omniface::ui {

class DetailPage : public QWidget {
    Q_OBJECT

public:
    explicit DetailPage(QWidget* parent = nullptr);

    void showTask(const std::string& output_dir, bool from_history = false);

signals:
    void backRequested();
    void backToHistory();
    void deleteRequested(const std::string& dir_path);

private slots:
    void onDetailFrameSync(qint64 positionMs);

private:
    void setupUI();
    void loadCSV(const std::string& output_dir);
    QImage matToQImage(const cv::Mat& bgr);

    VideoPlayerWidget* detail_video_ = nullptr;
    VideoPlayerWidget* detail_orig_video_ = nullptr;
    QLabel* detail_ref_thumb_ = nullptr;
    QLabel* detail_ref_detected_ = nullptr;
    QLabel* detail_info_label_ = nullptr;
    QLabel* detail_face_thumb_ = nullptr;
    QLabel* detail_data_label_ = nullptr;

    std::vector<std::string> csv_headers_;
    std::vector<std::vector<std::string>> csv_rows_;
    std::unordered_map<int, int> frame_to_row_;
    std::vector<std::string> aligned_paths_;
    double tracked_fps_ = 0.0;
    bool from_history_ = false;
    std::string current_dir_;

    QImage qt_image_cache_;
};

}  // namespace omniface::ui
