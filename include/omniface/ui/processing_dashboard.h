#pragma once

#include <cstdint>
#include <string>

#include <QString>
#include <QWidget>

namespace omniface::ui {

struct DashboardData {
    double pipeline_fps = 0.0;
    double recognition_fps = 0.0;
    double analysis_fps = 0.0;
    bool analysis_running = false;

    int64_t processed_frames = 0;
    int64_t face_detections = 0;
    int64_t target_matches = 0;
    int64_t openface_analyses = 0;
    int64_t onnx_analyses = 0;
    int64_t elapsed_seconds = 0;
    std::string analysis_label;
};

class FpsPanel : public QWidget {
    Q_OBJECT

public:
    explicit FpsPanel(QWidget* parent = nullptr);
    void updateData(const DashboardData& data);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

private:
    double smooth_pipe_ = 0.0;
    double smooth_rec_ = 0.0;
    double smooth_ana_ = 0.0;
    bool analysis_running_ = false;
    QString analysis_label_;

    static constexpr double kAlpha = 0.3;
};

class CounterPanel : public QWidget {
    Q_OBJECT

public:
    explicit CounterPanel(QWidget* parent = nullptr);
    void updateData(const DashboardData& data);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

private:
    DashboardData data_;
};

}  // namespace omniface::ui
