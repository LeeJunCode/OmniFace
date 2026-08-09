#pragma once

#include "omniface/core/quality/l2cs/l2cs_quality_types.h"
#include "omniface/core/quality/mediapipe/mediapipe_quality_types.h"
#include "omniface/core/quality/mefarg_au/mefarg_au_quality_types.h"
#include "omniface/core/quality/openface/openface_quality_types.h"

#include <QLabel>
#include <QScrollArea>
#include <QTextEdit>
#include <QWidget>

namespace QtCharts {
class QChartView;
}

namespace omniface::ui {

class QualityReportWidget : public QWidget {
    Q_OBJECT
public:
    explicit QualityReportWidget(QWidget* parent = nullptr);

    void RenderReport(const quality::OpenFaceQualityReport& report);
    void RenderMediaPipeReport(const quality::MediaPipeQualityReport& report);
    void RenderL2csReport(const quality::L2csQualityReport& report);
    void RenderMefargAuReport(const quality::MefargAuQualityReport& report);
    void SetStatusMessage(const QString& msg);
    void Clear();

private:
    void SetupUI();
    void BuildConfidenceChart(const quality::OpenFaceQualityReport& r);
    void BuildConfidenceHistogram(const quality::OpenFaceQualityReport& r);
    void BuildPoseChart(const quality::OpenFaceQualityReport& r);

    void BuildGazeChart(const quality::OpenFaceQualityReport& r);
    void BuildGradePie(const quality::OpenFaceQualityReport& r);
    void BuildAuBarChart(const quality::OpenFaceQualityReport& r);
    void BuildScorecard(const quality::OpenFaceQualityReport& r);

    QLabel* summary_label_ = nullptr;
    QLabel* grade_dist_label_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QWidget* content_widget_ = nullptr;
};

}  // namespace omniface::ui
