#pragma once

#include "omniface/core/quality/base_quality_evaluator.h"
#include "omniface/core/quality/mefarg_au/mefarg_au_quality_types.h"

#include <QObject>
#include <QString>

#include <unordered_map>
#include <vector>

namespace omniface::quality {

class MefargAuQualityEvaluator : public BaseQualityEvaluator {
    Q_OBJECT
public:
    explicit MefargAuQualityEvaluator(QObject* parent = nullptr);

    bool LoadCsv(const QString& main_csv, const QString& quality_csv) override;
    void Evaluate() override;
    bool IsReady() const override;

    const MefargAuQualityReport& Report() const { return report_; }

signals:
    void Finished(const MefargAuQualityReport& report);
    void Error(const QString& message);

private:
    struct RawFrame {
        int frame_number = 0;
        int active_au_count = 0;
        float mean_decision_margin = 0;
    };

    std::unordered_map<std::string, int> BuildColumnMap(const QStringList& header);
    RawFrame ParseRow(const QStringList& cols,
                     const std::unordered_map<std::string, int>& cm);

    void AssignFrameGrades();
    void GenerateSummary();

    std::vector<RawFrame> raw_frames_;
    MefargAuQualityReport report_;
    bool loaded_ = false;
    bool evaluated_ = false;
};

}  // namespace omniface::quality
