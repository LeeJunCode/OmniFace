#pragma once

#include "omniface/core/quality/interface/base_quality_evaluator.h"
#include "omniface/core/quality/mediapipe/mediapipe_quality_types.h"

#include <QObject>
#include <QString>

#include <unordered_map>
#include <vector>

namespace omniface::quality {

class MediaPipeQualityEvaluator : public BaseQualityEvaluator {
    Q_OBJECT
public:
    explicit MediaPipeQualityEvaluator(QObject* parent = nullptr);

    bool LoadCsv(const QString& main_csv, const QString& quality_csv) override;
    void Evaluate() override;
    bool IsReady() const override;

    const MediaPipeQualityReport& Report() const { return report_; }

signals:
    void Finished(const MediaPipeQualityReport& report);
    void Error(const QString& message);

private:
    struct RawFrame {
        int frame_number = 0;
        float timestamp = 0;
        float reproj_rms = 0;
        float face_size = 0;
        float pitch_deg = 0, yaw_deg = 0, roll_deg = 0;
    };

    std::unordered_map<std::string, int> BuildColumnMap(const QStringList& header);
    RawFrame ParseRow(const QStringList& cols,
                      const std::unordered_map<std::string, int>& cm);

    void AssignFrameGrades();
    void GenerateSummary();

    std::vector<RawFrame> raw_frames_;
    MediaPipeQualityReport report_;
    bool loaded_ = false;
    bool evaluated_ = false;
};

}  // namespace omniface::quality
