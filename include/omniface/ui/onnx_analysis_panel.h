#pragma once

#include <vector>

#include <QWidget>

#include "omniface/core/analysis/l2cs/l2cs_estimator.h"
#include "omniface/core/analysis/mediapipe/mediapipe_analyzer.h"

class QLabel;
class QVBoxLayout;

namespace omniface::ui {

class OnnxAnalysisPanel : public QWidget {
    Q_OBJECT

public:
    explicit OnnxAnalysisPanel(QWidget* parent = nullptr);

    void updateResults(const std::vector<analysis::mediapipe::MediaPipeAnalysisResult>& results);
    void updateGazeResults(const std::vector<analysis::l2cs::L2csEstimationResult>& results);
    void setAnalysisLabel(const QString& label);
    void clear();
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override;

private:
    std::vector<analysis::mediapipe::MediaPipeAnalysisResult> results_;
    QLabel* title_label_;
    QLabel* data_label_;
};

}  // namespace omniface::ui
