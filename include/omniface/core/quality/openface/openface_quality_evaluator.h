#pragma once

#include "omniface/core/quality/interface/base_quality_evaluator.h"
#include "omniface/core/quality/openface/openface_quality_types.h"

#include <QObject>
#include <QString>

#include <unordered_map>
#include <vector>

namespace omniface::quality {

class OpenFaceQualityEvaluator : public BaseQualityEvaluator {
    Q_OBJECT
public:
    explicit OpenFaceQualityEvaluator(QObject* parent = nullptr);

    bool LoadCsv(const QString& main_csv, const QString& quality_csv) override;
    void Evaluate() override;
    bool IsReady() const override;

    const OpenFaceQualityReport& Report() const { return report_; }

signals:
    void Finished(const OpenFaceQualityReport& report);
    void Error(const QString& message);

private:
    struct RawFrame {
        int frame_number = 0;
        float confidence = 0;
        bool success = false;
        float pose_rx = 0, pose_ry = 0, pose_rz = 0;
        float gaze_0_x = 0, gaze_0_y = 0, gaze_0_z = 0;
        float gaze_1_x = 0, gaze_1_y = 0, gaze_1_z = 0;
        float gaze_angle_x = 0, gaze_angle_y = 0;
        std::vector<int> active_aus;
        std::vector<float> au_intensities;
        std::vector<float> eye_lmk_x;
        std::vector<float> eye_lmk_y;
        std::vector<float> pdm_params;
        float quality_score = 0;
        int visible_landmarks = 68;
    };

    struct QualityCsvData {
        std::vector<float> eigenvalues;
        std::unordered_map<int, float> frame_quality_score;
        std::unordered_map<int, int> frame_visible;
        bool valid = false;
    };

    std::unordered_map<std::string, int> BuildColumnMap(const QStringList& header);
    RawFrame ParseMainRow(const QStringList& cols,
                          const std::unordered_map<std::string, int>& cm);
    QualityCsvData LoadQualityCsv(const QString& path);

    void EvaluateLandmarkQuality();
    void EvaluatePoseQuality();

    void EvaluateGazeQuality();
    void EvaluateAuQuality();
    void AssignFrameGrades();
    void GenerateSummary();

    std::vector<RawFrame> raw_frames_;
    QualityCsvData quality_data_;
    OpenFaceQualityReport report_;
    bool loaded_ = false;
    bool evaluated_ = false;
};

}  // namespace omniface::quality
