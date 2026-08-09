#pragma once

#include <QObject>
#include <QString>

namespace omniface::quality {

class BaseQualityEvaluator : public QObject {
    Q_OBJECT
public:
    ~BaseQualityEvaluator() override = default;

    virtual bool LoadCsv(const QString& main_csv, const QString& quality_csv) = 0;
    virtual void Evaluate() = 0;
    virtual bool IsReady() const = 0;
};

}  // namespace omniface::quality
