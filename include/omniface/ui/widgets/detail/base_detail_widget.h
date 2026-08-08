#pragma once

#include <QWidget>

#include <string>

namespace omniface {
namespace ui {

class BaseDetailWidget : public QWidget {
    Q_OBJECT
public:
    explicit BaseDetailWidget(QWidget* parent = nullptr) : QWidget(parent) {}
    ~BaseDetailWidget() override = default;

    virtual void SetCsvPath(const QString& path) { csv_path_ = path; }
    virtual void SetOutputDir(const QString& path) { output_dir_ = path; }
    virtual void SetAlignedFaceDir(const QString& path) { aligned_face_dir_ = path; }
    virtual void LoadCsvData() = 0;
    virtual void UpdateForFrame(int frameIndex) = 0;
    virtual int TotalFrames() const = 0;

protected:
    QString csv_path_;
    QString output_dir_;
    QString aligned_face_dir_;
};

}  // namespace ui
}  // namespace omniface
