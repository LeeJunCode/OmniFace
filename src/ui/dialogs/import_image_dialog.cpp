#include "omniface/ui/dialogs/import_image_dialog.h"
#include "omniface/ui/theme.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/align.h"
#include "omniface/core/recognition/face_recognizer.h"
#include "omniface/db/image_repo.h"
#include "omniface/db/snowflake.h"

namespace omniface {
namespace ui {

static QString DataDir() {
    QString dir = QApplication::applicationDirPath() + QStringLiteral("/data/images");
    QDir().mkpath(dir);
    return dir;
}

static QString ResolveModelPath(const QString& relative) {
    QString appDir = QApplication::applicationDirPath();
    QString path = appDir + QStringLiteral("/") + relative;
    if (QFile::exists(path)) return QFileInfo(path).absoluteFilePath();
    path = appDir + QStringLiteral("/../") + relative;
    if (QFile::exists(path)) return QFileInfo(path).absoluteFilePath();
    path = appDir + QStringLiteral("/../../") + relative;
    if (QFile::exists(path)) return QFileInfo(path).absoluteFilePath();
    return relative;
}

ImportImageDialog::ImportImageDialog(db::ImageRepo* repo, QWidget* parent)
    : QDialog(parent), repo_(repo) {
    auto& tm = ThemeManager::Instance();
    setWindowTitle(QStringLiteral("导入图片"));
    setMinimumSize(550, 550);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QLabel { color: %2; }").arg(tm.SurfaceBg(), tm.TextPrimary()));

    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);

    // ── File selection ──────────────────────────────────────

    auto* file_layout = new QHBoxLayout();
    path_edit_ = new QLineEdit(this);
    path_edit_->setPlaceholderText(QStringLiteral("选择图片文件..."));
    path_edit_->setReadOnly(true);
    path_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 8px; color: %3; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary()));
    file_layout->addWidget(path_edit_, 1);

    auto* browse_btn = new QPushButton(QStringLiteral("浏览..."), this);
    browse_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 14px; }"
        "QPushButton:hover { background: %4; }").arg("transparent", tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(browse_btn, &QPushButton::clicked, this, &ImportImageDialog::OnSelectFile);
    file_layout->addWidget(browse_btn);
    root->addLayout(file_layout);

    // ── Preview ─────────────────────────────────────────────

    preview_label_ = new QLabel(this);
    preview_label_->setAlignment(Qt::AlignCenter);
    preview_label_->setMinimumHeight(200);
    preview_label_->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border: 1px solid %2; border-radius: 4px; }").arg(tm.DeepBg(), tm.Border()));
    root->addWidget(preview_label_, 1);

    // ── Status ──────────────────────────────────────────────

    status_label_ = new QLabel(QStringLiteral("请选择图片文件"), this);
    status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.TextMuted()));
    root->addWidget(status_label_);

    // ── Face list ───────────────────────────────────────────

    face_list_ = new QListWidget(this);
    face_list_->setMaximumHeight(80);
    face_list_->setVisible(false);
    face_list_->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; color: %3; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:selected { background: %4; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.SelectedBg()));
    connect(face_list_, &QListWidget::currentRowChanged, this, &ImportImageDialog::OnFaceSelected);
    root->addWidget(face_list_);

    // ── Name input ──────────────────────────────────────────

    auto* edit_form = new QFormLayout();
    name_edit_ = new QLineEdit(this);
    name_edit_->setPlaceholderText(QStringLiteral("图片名称（默认取文件名）"));
    name_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 8px; color: %3; }"
        "QLineEdit:focus { border-color: %4; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    edit_form->addRow(QStringLiteral("图片名称:"), name_edit_);

    description_edit_ = new QLineEdit(this);
    description_edit_->setPlaceholderText(QStringLiteral("图片描述（可选）"));
    description_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 8px; color: %3; }"
        "QLineEdit:focus { border-color: %4; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    edit_form->addRow(QStringLiteral("描述:"), description_edit_);

    notes_edit_ = new QLineEdit(this);
    notes_edit_->setPlaceholderText(QStringLiteral("备注（可选）"));
    notes_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 8px; color: %3; }"
        "QLineEdit:focus { border-color: %4; }").arg(tm.DeepBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    edit_form->addRow(QStringLiteral("备注:"), notes_edit_);
    root->addLayout(edit_form);

    // ── Buttons ─────────────────────────────────────────────

    auto* btn_layout = new QHBoxLayout();
    btn_layout->addStretch();

    auto* cancel_btn = new QPushButton(QStringLiteral("取消"), this);
    cancel_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 8px 20px; }"
        "QPushButton:hover { background: %3; }").arg("transparent", tm.TextPrimary(), tm.HoverBg()));
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn);

    import_btn_ = new QPushButton(QStringLiteral("导入"), this);
    import_btn_->setEnabled(false);
    import_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none;"
        "  border-radius: 4px; padding: 8px 20px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:disabled { background: %4; color: %5; }").arg(tm.Primary(), tm.White(), tm.PrimaryHover(), tm.HoverBg(), tm.TextDim()));
    connect(import_btn_, &QPushButton::clicked, this, &ImportImageDialog::OnImport);
    btn_layout->addWidget(import_btn_);

    root->addLayout(btn_layout);
}

ImportImageDialog::~ImportImageDialog() {
    if (result() != QDialog::Accepted && !copied_path_.isEmpty()) {
        QFile::remove(copied_path_);
    }
}

void ImportImageDialog::OnSelectFile() {
    auto& tm = ThemeManager::Instance();
    QString path = QFileDialog::getOpenFileName(this,
        QStringLiteral("选择图片文件"), {},
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.bmp *.tiff *.webp);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    // Clean up previous temp copy
    if (!copied_path_.isEmpty()) {
        QFile::remove(copied_path_);
        copied_path_.clear();
    }

    source_path_ = path;
    path_edit_->setText(path);

    QFileInfo fi(path);
    name_edit_->setText(fi.completeBaseName());

    // Copy to data dir with pure ASCII name before any OpenCV access
    copied_id_ = db::Snowflake::Instance().NextId();
    QString ext = fi.suffix();
    copied_path_ = DataDir() + QStringLiteral("/") + QStringLiteral("%1.%2").arg(copied_id_).arg(ext);

    if (!QFile::copy(path, copied_path_)) {
        status_label_->setText(QStringLiteral("无法复制文件到管理目录"));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        import_btn_->setEnabled(false);
        copied_path_.clear();
        return;
    }

    // Read from the ASCII-safe copy
    source_image_ = cv::imread(copied_path_.toStdString());
    if (source_image_.empty()) {
        status_label_->setText(QStringLiteral("无法加载图片文件"));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        import_btn_->setEnabled(false);
        return;
    }

    cv::Mat rgb;
    try {
        cv::cvtColor(source_image_, rgb, cv::COLOR_BGR2RGB);
    } catch (const std::exception& e) {
        status_label_->setText(QStringLiteral("图片格式转换失败: %1").arg(QString::fromStdString(e.what())));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        import_btn_->setEnabled(false);
        return;
    }
    QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    source_pixmap_ = QPixmap::fromImage(qimg.copy());
    preview_label_->setPixmap(source_pixmap_.scaled(320, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    RunDetection();
}

void ImportImageDialog::RunDetection() {
    auto& tm = ThemeManager::Instance();
    detected_faces_.clear();
    face_list_->clear();
    face_list_->setVisible(false);
    selected_face_index_ = -1;
    selected_embedding_ = {};
    import_btn_->setEnabled(false);

    if (!detector_) {
        std::string detModel = ResolveModelPath(QStringLiteral("models/buffalo_sc/det_500m.onnx")).toStdString();
        if (!QFile::exists(QString::fromStdString(detModel))) {
            status_label_->setText(QStringLiteral("人脸检测模型未找到"));
            status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
            return;
        }
        try {
            detection::FaceDetector::Config detCfg;
            detCfg.confidence_threshold = 0.5f;
            detCfg.nms_threshold = 0.4f;
            detector_ = std::make_unique<detection::FaceDetector>(detModel, detCfg);
        } catch (const std::exception& e) {
            status_label_->setText(QStringLiteral("加载检测模型失败: %1").arg(QString::fromStdString(e.what())));
            status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
            return;
        } catch (...) {
            status_label_->setText(QStringLiteral("加载检测模型时发生未知错误"));
            status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
            return;
        }
    }

    try {
        detected_faces_ = detector_->Detect(source_image_);
    } catch (const std::exception& e) {
        status_label_->setText(QStringLiteral("人脸检测失败: %1").arg(QString::fromStdString(e.what())));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        return;
    } catch (...) {
        status_label_->setText(QStringLiteral("人脸检测时发生未知错误"));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        return;
    }

    if (detected_faces_.empty()) {
        status_label_->setText(QStringLiteral("未检测到人脸，请换一张清晰的正面照片"));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        DrawFaceBox(-1);
        return;
    }

    DrawFaceBox(0);

    if (detected_faces_.size() == 1) {
        status_label_->setText(QStringLiteral("检测到 1 张人脸，置信度: %1").arg(FormatConfidence(detected_faces_[0].confidence)));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.Success()));

        face_list_->addItem(QStringLiteral("人脸 1 — 置信度: %1").arg(FormatConfidence(detected_faces_[0].confidence)));
        face_list_->setCurrentRow(0);
        OnFaceSelected(0);
    } else {
        status_label_->setText(QStringLiteral("检测到 %1 张人脸，请选择保留哪一张").arg(static_cast<int>(detected_faces_.size())));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.Warning()));

        face_list_->setVisible(true);
        for (size_t i = 0; i < detected_faces_.size(); ++i) {
            face_list_->addItem(QStringLiteral("人脸 %1 — 置信度: %2 — 位置: (%3, %4)")
                .arg(static_cast<int>(i + 1))
                .arg(FormatConfidence(detected_faces_[i].confidence))
                .arg(static_cast<int>(detected_faces_[i].x1))
                .arg(static_cast<int>(detected_faces_[i].y1)));
        }
        face_list_->setCurrentRow(0);
    }
}

void ImportImageDialog::DrawFaceBox(int highlightIndex) {
    if (source_pixmap_.isNull()) return;

    QPixmap px = source_pixmap_;
    QPainter p(&px);
    for (size_t i = 0; i < detected_faces_.size(); ++i) {
        const auto& b = detected_faces_[i];
        QColor color = (static_cast<int>(i) == highlightIndex) ? QColor(ThemeManager::Instance().Primary()) : QColor(ThemeManager::Instance().TextDim());
        QPen pen(color, 2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(b.x1, b.y1, b.x2 - b.x1, b.y2 - b.y1));

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(ThemeManager::Instance().Primary()));
        for (int k = 0; k < 5; ++k) {
            float kx = b.keypoints[k * 2];
            float ky = b.keypoints[k * 2 + 1];
            if (kx > 0 && ky > 0)
                p.drawEllipse(QPointF(kx, ky), 2, 2);
        }

        if (detected_faces_.size() > 1) {
            p.setPen(Qt::white);
            p.setFont(QFont(QStringLiteral("Arial"), 10));
            p.drawText(QPointF(b.x1 + 4, b.y1 - 6),
                QStringLiteral("%1: %2").arg(static_cast<int>(i + 1)).arg(FormatConfidence(b.confidence)));
        }
    }
    p.end();

    preview_label_->setPixmap(px.scaled(320, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QString ImportImageDialog::FormatConfidence(float conf) {
    return QStringLiteral("%1%").arg(conf * 100.0f, 0, 'f', 1);
}

void ImportImageDialog::OnFaceSelected(int index) {
    auto& tm = ThemeManager::Instance();
    if (index < 0 || static_cast<size_t>(index) >= detected_faces_.size()) return;

    selected_face_index_ = index;
    DrawFaceBox(index);

    if (!recognizer_) {
        std::string recModel = ResolveModelPath(QStringLiteral("models/buffalo_sc/w600k_mbf.onnx")).toStdString();
        if (!QFile::exists(QString::fromStdString(recModel))) {
            status_label_->setText(QStringLiteral("人脸识别模型未找到"));
            status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
            return;
        }
        try {
            recognition::FaceRecognizer::Config recCfg;
            recognizer_ = std::make_unique<recognition::FaceRecognizer>(recModel, recCfg);
        } catch (const std::exception& e) {
            status_label_->setText(QStringLiteral("加载识别模型失败: %1").arg(QString::fromStdString(e.what())));
            status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
            return;
        } catch (...) {
            status_label_->setText(QStringLiteral("加载识别模型时发生未知错误"));
            status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
            return;
        }
    }

    cv::Mat aligned;
    try {
        aligned = recognition::AlignFace(source_image_, detected_faces_[index], 112);
    } catch (const std::exception& e) {
        selected_embedding_ = {};
        import_btn_->setEnabled(false);
        status_label_->setText(QStringLiteral("人脸对齐失败: %1").arg(QString::fromStdString(e.what())));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        return;
    } catch (...) {
        selected_embedding_ = {};
        import_btn_->setEnabled(false);
        status_label_->setText(QStringLiteral("人脸对齐时发生未知错误"));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        return;
    }

    if (aligned.empty()) {
        selected_embedding_ = {};
        import_btn_->setEnabled(false);
        return;
    }

    try {
        selected_embedding_ = recognizer_->Extract(aligned);
    } catch (const std::exception& e) {
        selected_embedding_ = {};
        import_btn_->setEnabled(false);
        status_label_->setText(QStringLiteral("特征提取失败: %1").arg(QString::fromStdString(e.what())));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        return;
    } catch (...) {
        selected_embedding_ = {};
        import_btn_->setEnabled(false);
        status_label_->setText(QStringLiteral("特征提取时发生未知错误"));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
        return;
    }
    if (selected_embedding_.valid) {
        import_btn_->setEnabled(true);
        status_label_->setText(QStringLiteral("人脸 %1 — 置信度: %2 — embedding 已提取")
            .arg(index + 1)
            .arg(FormatConfidence(detected_faces_[index].confidence)));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.Success()));
    } else {
        import_btn_->setEnabled(false);
        status_label_->setText(QStringLiteral("人脸特征提取失败"));
        status_label_->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.ErrorBright()));
    }
}


void ImportImageDialog::OnImport() {
    if (copied_path_.isEmpty() || !selected_embedding_.valid) return;
    if (selected_face_index_ < 0 || static_cast<size_t>(selected_face_index_) >= detected_faces_.size()) return;

    const auto& box = detected_faces_[selected_face_index_];

    db::ImageRecord rec;
    rec.id = copied_id_;
    rec.name = name_edit_->text().trimmed().toStdString();
    if (rec.name.empty()) {
        QFileInfo fi(source_path_);
        rec.name = fi.completeBaseName().toStdString();
    }
    rec.description = description_edit_->text().trimmed().toStdString();
    rec.notes = notes_edit_->text().trimmed().toStdString();
    rec.saved_path = copied_path_.toStdString();
    rec.face_confidence = box.confidence;
    rec.bbox_x1 = box.x1;
    rec.bbox_y1 = box.y1;
    rec.bbox_x2 = box.x2;
    rec.bbox_y2 = box.y2;
    rec.keypoints = box.keypoints;
    rec.embedding = selected_embedding_.features;
    rec.created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();

    imported_id_ = 0;
    try {
        imported_id_ = repo_->Insert(rec);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("写入数据库失败:\n%1").arg(QString::fromStdString(e.what())));
        QFile::remove(copied_path_);
        copied_path_.clear();
        return;
    }
    if (imported_id_ > 0) {
        copied_path_.clear();  // File is now managed, don't clean up
        QMessageBox::information(this, QStringLiteral("导入成功"),
            QStringLiteral("图片 \"%1\" 已成功导入。").arg(QString::fromStdString(rec.name)));
        accept();
    } else {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
            QStringLiteral("写入数据库失败。"));
        QFile::remove(copied_path_);
        copied_path_.clear();
    }
}

}  // namespace ui
}  // namespace omniface
