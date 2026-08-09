#pragma once

#include <QObject>

#include "omniface/core/quality/base_data_cleaner.h"
#include "omniface/core/quality/l2cs/l2cs_quality_types.h"
#include "omniface/core/quality/quality_types.h"

namespace omniface::clean {

class L2csDataCleaner : public BaseDataCleaner {
    Q_OBJECT
public:
    explicit L2csDataCleaner(QObject* parent = nullptr);

    bool LoadCsv(const QString& main_csv, const QString& quality_csv) override;
    CleanResult Execute(const CleanConfig& cfg) override;

private:
    quality::L2csQualityReport report_;
    QString main_csv_path_;
    bool loaded_ = false;

    void WriteCleanCsv(const std::vector<size_t>& keep_indices,
                      const QString& output_path);
};

}  // namespace omniface::clean
