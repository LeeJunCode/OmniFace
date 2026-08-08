#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

#include <memory>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "omniface/types.h"

namespace omniface {
namespace db {
class ImageRepo;
}
namespace detection {
class FaceDetector;
}
namespace recognition {
class FaceRecognizer;
}

namespace ui {

class ImportImageDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImportImageDialog(db::ImageRepo* repo, QWidget* parent = nullptr);
    ~ImportImageDialog() override;

    int64_t ImportedId() const { return imported_id_; }

private slots:
    void OnSelectFile();
    void OnFaceSelected(int index);
    void OnImport();

private:
    void RunDetection();
    void DrawFaceBox(int highlightIndex);
    static QString FormatConfidence(float conf);

    db::ImageRepo* repo_;
    int64_t imported_id_ = 0;

    std::unique_ptr<detection::FaceDetector> detector_;
    std::unique_ptr<recognition::FaceRecognizer> recognizer_;
    std::vector<BoundingBox> detected_faces_;
    FaceEmbedding selected_embedding_;
    int selected_face_index_ = -1;

    QLabel* preview_label_ = nullptr;
    QLineEdit* path_edit_ = nullptr;
    QLabel* status_label_ = nullptr;
    QListWidget* face_list_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* description_edit_ = nullptr;
    QLineEdit* notes_edit_ = nullptr;
    QPushButton* import_btn_ = nullptr;

    QString source_path_;
    QString copied_path_;
    int64_t copied_id_ = 0;
    QPixmap source_pixmap_;
    cv::Mat source_image_;
};

}  // namespace ui
}  // namespace omniface
