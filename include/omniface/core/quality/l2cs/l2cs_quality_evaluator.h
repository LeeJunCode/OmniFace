#pragma once

#include "omniface/core/quality/interface/base_quality_evaluator.h"
#include "omniface/core/quality/l2cs/l2cs_quality_types.h"

#include <QObject>
#include <QString>

#include <unordered_map>
#include <vector>

namespace omniface::quality {

class L2csQualityEvaluator : public BaseQualityEvaluator {
    Q_OBJECT
public:
    explicit L2csQualityEvaluator(QObject* parent = nullptr);

    bool LoadCsv(const QString& main_csv, const QString& quality_csv) override;
    void Evaluate() override;
    bool IsReady() const override;

    const L2csQualityReport& Report() const { return report_; }

signals:
    void Finished(const L2csQualityReport& report);
    void Error(const QString& message);

private:
    struct RawFrame {
        int frame_number = 0;
        float timestamp = 0;
        float yaw_deg = 0, pitch_deg = 0;
        float yaw_maxp = 0, pitch_maxp = 0;
    };

    std::unordered_map<std::string, int> BuildColumnMap(const QStringList& header);
    RawFrame ParseRow(const QStringList& cols,
                     const std::unordered_map<std::string, int>& cm);

    void AssignFrameGrades();
    void GenerateSummary();

    std::vector<RawFrame> raw_frames_;
    L2csQualityReport report_;
    bool loaded_ = false;
    bool evaluated_ = false;
};

}  // namespace omniface::quality
