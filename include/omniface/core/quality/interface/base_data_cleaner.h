#pragma once

#include <QObject>
#include <QString>

#include "omniface/core/quality/interface/quality_types.h"

namespace omniface::clean {

class BaseDataCleaner : public QObject {
    Q_OBJECT
public:
    ~BaseDataCleaner() override = default;

    virtual bool LoadCsv(const QString& main_csv, const QString& quality_csv) = 0;
    virtual CleanResult Execute(const CleanConfig& cfg) = 0;

signals:
    void Finished(const CleanResult& result);
    void Error(const QString& message);
};

}  // namespace omniface::clean
