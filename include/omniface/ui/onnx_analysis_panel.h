#pragma once

#include <vector>

#include <QWidget>

#include "omniface/core/analysis/onnx/onnx_face_analyzer.h"

class QLabel;
class QVBoxLayout;

namespace omniface::ui {

class OnnxAnalysisPanel : public QWidget {
    Q_OBJECT

public:
    explicit OnnxAnalysisPanel(QWidget* parent = nullptr);

    void updateResults(const std::vector<analysis::onnx::OnnxAnalysisResult>& results);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override;

private:
    std::vector<analysis::onnx::OnnxAnalysisResult> results_;
    QLabel* title_label_;
    QLabel* data_label_;
};

}  // namespace omniface::ui
