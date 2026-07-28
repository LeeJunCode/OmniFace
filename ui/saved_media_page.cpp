#include "omniface/ui/saved_media_page.h"
#include "omniface/ui/image_utils.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QUrl>

#include "omniface/ui/stylesheet.h"

namespace omniface::ui {

SavedMediaPage::SavedMediaPage(MediaType type, QWidget* parent)
    : QWidget(parent), type_(type) {
    setStyleSheet("background-color: #f5f5f5;");

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(8);

    auto* top = new QHBoxLayout;
    auto* back_btn = new QPushButton(QStringLiteral("\u2190 \u8fd4\u56de"));
    back_btn->setFixedHeight(28);
    connect(back_btn, &QPushButton::clicked, this, &SavedMediaPage::backRequested);
    top->addWidget(back_btn);
    top->addStretch();
    vl->addLayout(top);

    if (type_ == MediaType::Video) {
        video_preview_ = new VideoPlayerWidget;
        video_preview_->setMinimumHeight(200);
        video_preview_->setMaximumHeight(280);
        video_preview_->hide();
        vl->addWidget(video_preview_);
    }

    // Batch action bar (hidden when empty)
    batch_bar_ = new QWidget;
    batch_bar_->setStyleSheet("background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 6px;");
    auto* batch_layout = new QHBoxLayout(batch_bar_);
    batch_layout->setContentsMargins(12, 6, 12, 6);
    batch_layout->setSpacing(12);

    select_all_cb_ = new QCheckBox(QStringLiteral("\u5168\u9009"));
    select_all_cb_->setStyleSheet(
        "QCheckBox { font-size: 13px; color: #333; border: none; spacing: 6px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 2px solid #bdbdbd; "
        "border-radius: 3px; background-color: #fafafa; }"
        "QCheckBox::indicator:hover { border-color: #1565c0; }"
        "QCheckBox::indicator:checked { background-color: #1565c0; border-color: #1565c0; }");
    connect(select_all_cb_, &QCheckBox::clicked, this, &SavedMediaPage::onSelectAll);
    batch_layout->addWidget(select_all_cb_);

    batch_del_btn_ = new QPushButton(QStringLiteral("\u6279\u91cf\u5220\u9664"));
    batch_del_btn_->setFixedHeight(30);
    batch_del_btn_->setStyleSheet(
        "QPushButton { background-color: #e53935; color: white; border: none; "
        "border-radius: 4px; padding: 4px 16px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #c62828; }"
        "QPushButton:disabled { background-color: #ccc; }");
    connect(batch_del_btn_, &QPushButton::clicked, this, &SavedMediaPage::onBatchDelete);
    batch_layout->addWidget(batch_del_btn_);
    batch_layout->addStretch();

    batch_bar_->hide();
    vl->addWidget(batch_bar_);

    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    content_ = new QWidget;
    content_->setStyleSheet("background: transparent;");
    auto* vc_layout = new QVBoxLayout(content_);
    vc_layout->setContentsMargins(0, 0, 0, 0);
    vc_layout->setSpacing(8);
    scroll_->setWidget(content_);
    vl->addWidget(scroll_, 1);
}

QImage SavedMediaPage::matToQImage(const cv::Mat& bgr) {
    return MatToQImage(bgr, qt_image_cache_);
}

void SavedMediaPage::clearContent() {
    if (content_->layout()) {
        QLayoutItem* item;
        while ((item = content_->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }
}

void SavedMediaPage::onSelectAll() {
    bool checked = select_all_cb_->isChecked();
    if (checked) {
        selected_.clear();
        auto* layout = static_cast<QVBoxLayout*>(content_->layout());
        for (int i = 0; i < layout->count(); ++i) {
            auto* frame = qobject_cast<QFrame*>(layout->itemAt(i)->widget());
            if (!frame) continue;
            auto* cb = frame->findChild<QCheckBox*>("mediaCb");
            if (cb) {
                cb->setChecked(true);
                selected_.insert(cb->property("file_path").toString().toStdString());
            }
        }
    } else {
        auto* layout = static_cast<QVBoxLayout*>(content_->layout());
        for (int i = 0; i < layout->count(); ++i) {
            auto* frame = qobject_cast<QFrame*>(layout->itemAt(i)->widget());
            if (!frame) continue;
            auto* cb = frame->findChild<QCheckBox*>("mediaCb");
            if (cb) cb->setChecked(false);
        }
        selected_.clear();
    }
    updateBatchButtons();
}

void SavedMediaPage::onBatchDelete() {
    if (selected_.empty()) return;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("\u786e\u8ba4\u6279\u91cf\u5220\u9664"));
    msgBox.setText(QStringLiteral("\u786e\u5b9a\u8981\u5220\u9664\u6240\u9009 %1 \u4e2a\u6587\u4ef6\u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002").arg(static_cast<int>(selected_.size())));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setStyleSheet(ConfirmDialogStyleSheet());
    if (msgBox.exec() != QMessageBox::Yes) return;

    namespace fs = std::filesystem;
    for (const auto& path : selected_) {
        std::error_code ec;
        fs::remove(path, ec);
    }
    if (video_preview_) {
        video_preview_->stop();
        video_preview_->hide();
    }
    selected_.clear();
    refresh();
}

void SavedMediaPage::updateBatchButtons() {
    int n = static_cast<int>(selected_.size());
    batch_del_btn_->setEnabled(n > 0);
    if (n > 0)
        batch_del_btn_->setText(QStringLiteral("\u6279\u91cf\u5220\u9664 (%1)").arg(n));
    else
        batch_del_btn_->setText(QStringLiteral("\u6279\u91cf\u5220\u9664"));
}

void SavedMediaPage::refresh() {
    clearContent();
    if (video_preview_) {
        video_preview_->stop();
        video_preview_->hide();
    }
    selected_.clear();
    total_items_ = 0;
    if (type_ == MediaType::Video) showVideos();
    else showImages();
}

void SavedMediaPage::showVideos() {
    namespace fs = std::filesystem;
    std::string app_dir = QCoreApplication::applicationDirPath().toStdString();
    fs::path upload_dir = fs::path(app_dir) / "data" / "upload" / "video";

    auto* vc_layout = static_cast<QVBoxLayout*>(content_->layout());

    std::vector<std::string> videos;
    if (fs::is_directory(upload_dir)) {
        for (const auto& entry : fs::directory_iterator(upload_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov")
                videos.push_back(entry.path().string());
        }
    }

    if (videos.empty()) {
        auto* empty_label = new QLabel(QStringLiteral("\u6682\u65e0\u4e0a\u4f20\u7684\u89c6\u9891"));
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("color: #999; font-size: 14px; padding: 60px 0;");
        vc_layout->addWidget(empty_label);
        vc_layout->addStretch();
        batch_bar_->hide();
        return;
    }

    total_items_ = static_cast<int>(videos.size());
    batch_bar_->show();
    select_all_cb_->setChecked(false);
    updateBatchButtons();

    std::sort(videos.begin(), videos.end());
    for (const auto& vpath : videos) {
        fs::path p(vpath);
        std::string fname = p.filename().string();

        auto* card = new QFrame;
        card->setStyleSheet(
            "QFrame { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 6px; }"
            "QFrame:hover { border-color: #90caf9; }");
        card->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(card, &QFrame::customContextMenuRequested, this,
                [this, vpath](const QPoint&) {
                    QMenu menu;
                    auto* open_dir = menu.addAction(
                        QStringLiteral("\u67e5\u770b\u6587\u4ef6\u6240\u5728\u76ee\u5f55"));
                    connect(open_dir, &QAction::triggered, this, [vpath]() {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(
                            QString::fromStdString(
                                std::filesystem::path(vpath).parent_path().string())));
                    });
                    menu.exec(QCursor::pos());
                });
        auto* card_layout = new QHBoxLayout(card);
        card_layout->setContentsMargins(12, 12, 16, 12);
        card_layout->setSpacing(10);

        auto* cb = new QCheckBox;
        cb->setObjectName("mediaCb");
        cb->setProperty("file_path", QString::fromStdString(vpath));
        cb->setStyleSheet(
            "QCheckBox { border: none; spacing: 0px; }"
            "QCheckBox::indicator { width: 18px; height: 18px; border: 2px solid #bdbdbd; "
            "border-radius: 3px; background-color: #fafafa; }"
            "QCheckBox::indicator:hover { border-color: #1565c0; }"
            "QCheckBox::indicator:checked { background-color: #1565c0; "
            "border-color: #1565c0; }");
        connect(cb, &QCheckBox::toggled, this, [this, vpath](bool checked) {
            if (checked)
                selected_.insert(vpath);
            else
                selected_.erase(vpath);
            updateBatchButtons();
        });
        card_layout->addWidget(cb);

        auto* name_label = new QLabel(QString::fromStdString(fname));
        name_label->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; border: none;");
        card_layout->addWidget(name_label, 1);

        std::string path_copy = vpath;
        auto* play_btn = new QPushButton(QStringLiteral("\u64ad\u653e"));
        play_btn->setFixedHeight(32);
        play_btn->setStyleSheet(
            "QPushButton { background-color: #43a047; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #388e3c; }");
        connect(play_btn, &QPushButton::clicked, this, [this, path_copy]() {
            video_preview_->setVideoPath(path_copy);
            video_preview_->show();
            video_preview_->play();
        });
        card_layout->addWidget(play_btn);

        auto* del_btn = new QPushButton(QStringLiteral("\u5220\u9664"));
        del_btn->setFixedHeight(32);
        del_btn->setStyleSheet(
            "QPushButton { background-color: #e53935; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #c62828; }");
        connect(del_btn, &QPushButton::clicked, this, [this, path_copy]() {
            namespace fs = std::filesystem;
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QStringLiteral("\u786e\u8ba4\u5220\u9664"));
            msgBox.setText(QStringLiteral("\u786e\u5b9a\u8981\u5220\u9664\u8be5\u6587\u4ef6\u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"));
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::No);
            msgBox.setStyleSheet(ConfirmDialogStyleSheet());
            if (msgBox.exec() != QMessageBox::Yes) return;
            video_preview_->stop();
            video_preview_->hide();
            QTimer::singleShot(200, this, [this, path_copy]() {
                std::error_code ec;
                fs::remove(path_copy, ec);
                if (ec) {
                    spdlog::warn("Failed to delete {}: {}", path_copy, ec.message());
                    QMessageBox::warning(this,
                        QStringLiteral("\u5220\u9664\u5931\u8d25"),
                        QStringLiteral("\u6587\u4ef6\u6b63\u5728\u88ab\u4f7f\u7528\uff0c\u8bf7\u7a0d\u540e\u91cd\u8bd5\u3002"));
                }
                refresh();
            });
        });
        card_layout->addWidget(del_btn);

        vc_layout->addWidget(card);
    }
    vc_layout->addStretch();
}

void SavedMediaPage::showImages() {
    namespace fs = std::filesystem;
    std::string app_dir = QCoreApplication::applicationDirPath().toStdString();
    fs::path upload_dir = fs::path(app_dir) / "data" / "upload" / "img";

    auto* ic_layout = static_cast<QVBoxLayout*>(content_->layout());

    std::vector<std::string> images;
    if (fs::is_directory(upload_dir)) {
        for (const auto& entry : fs::directory_iterator(upload_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp")
                images.push_back(entry.path().string());
        }
    }

    if (images.empty()) {
        auto* empty_label = new QLabel(QStringLiteral("\u6682\u65e0\u4e0a\u4f20\u7684\u56fe\u7247"));
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("color: #999; font-size: 14px; padding: 60px 0;");
        ic_layout->addWidget(empty_label);
        ic_layout->addStretch();
        batch_bar_->hide();
        return;
    }

    total_items_ = static_cast<int>(images.size());
    batch_bar_->show();
    select_all_cb_->setChecked(false);
    updateBatchButtons();

    std::sort(images.begin(), images.end());
    for (const auto& img_path : images) {
        fs::path p(img_path);
        std::string fname = p.filename().string();

        auto* card = new QFrame;
        card->setStyleSheet(
            "QFrame { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 6px; }"
            "QFrame:hover { border-color: #90caf9; }");
        card->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(card, &QFrame::customContextMenuRequested, this,
                [this, img_path](const QPoint&) {
                    QMenu menu;
                    auto* open_dir = menu.addAction(
                        QStringLiteral("\u67e5\u770b\u6587\u4ef6\u6240\u5728\u76ee\u5f55"));
                    connect(open_dir, &QAction::triggered, this, [img_path]() {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(
                            QString::fromStdString(
                                std::filesystem::path(img_path).parent_path().string())));
                    });
                    menu.exec(QCursor::pos());
                });
        auto* card_layout = new QHBoxLayout(card);
        card_layout->setContentsMargins(12, 12, 16, 12);
        card_layout->setSpacing(10);

        auto* cb = new QCheckBox;
        cb->setObjectName("mediaCb");
        cb->setProperty("file_path", QString::fromStdString(img_path));
        cb->setStyleSheet(
            "QCheckBox { border: none; spacing: 0px; }"
            "QCheckBox::indicator { width: 18px; height: 18px; border: 2px solid #bdbdbd; "
            "border-radius: 3px; background-color: #fafafa; }"
            "QCheckBox::indicator:hover { border-color: #1565c0; }"
            "QCheckBox::indicator:checked { background-color: #1565c0; "
            "border-color: #1565c0; }");
        connect(cb, &QCheckBox::toggled, this, [this, img_path](bool checked) {
            if (checked)
                selected_.insert(img_path);
            else
                selected_.erase(img_path);
            updateBatchButtons();
        });
        card_layout->addWidget(cb);

        auto* thumb = new QLabel;
        thumb->setFixedSize(80, 80);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setStyleSheet(
            "background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px;");
        cv::Mat img_mat = cv::imread(img_path);
        if (!img_mat.empty()) {
            QImage qimg = matToQImage(img_mat);
            QPixmap pm = QPixmap::fromImage(qimg);
            if (!pm.isNull())
                thumb->setPixmap(pm.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            thumb->setText(QStringLiteral("\u56fe\u7247"));
        }
        card_layout->addWidget(thumb);

        auto* name_label = new QLabel(QString::fromStdString(fname));
        name_label->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; border: none;");
        card_layout->addWidget(name_label, 1);

        std::string path_copy = img_path;
        auto* view_btn = new QPushButton(QStringLiteral("\u67e5\u770b\u5927\u56fe"));
        view_btn->setFixedHeight(32);
        view_btn->setStyleSheet(
            "QPushButton { background-color: #1565c0; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #1976d2; }");
        connect(view_btn, &QPushButton::clicked, this, [this, path_copy]() {
            cv::Mat m = cv::imread(path_copy);
            if (!m.empty()) {
                QImage qimg = matToQImage(m);
                QDialog dlg(this);
                dlg.setWindowTitle(QStringLiteral("\u56fe\u7247\u9884\u89c8"));
                dlg.setStyleSheet("background-color: #ffffff;");
                dlg.setMinimumSize(400, 300);
                auto* dlg_layout = new QVBoxLayout(&dlg);
                auto* img_label = new QLabel;
                img_label->setPixmap(QPixmap::fromImage(qimg).scaled(
                    600, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                img_label->setAlignment(Qt::AlignCenter);
                dlg_layout->addWidget(img_label);
                dlg.exec();
            }
        });
        card_layout->addWidget(view_btn);

        auto* del_btn = new QPushButton(QStringLiteral("\u5220\u9664"));
        del_btn->setFixedHeight(32);
        del_btn->setStyleSheet(
            "QPushButton { background-color: #e53935; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #c62828; }");
        connect(del_btn, &QPushButton::clicked, this, [this, path_copy]() {
            namespace fs = std::filesystem;
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QStringLiteral("\u786e\u8ba4\u5220\u9664"));
            msgBox.setText(QStringLiteral("\u786e\u5b9a\u8981\u5220\u9664\u8be5\u6587\u4ef6\u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"));
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::No);
            msgBox.setStyleSheet(ConfirmDialogStyleSheet());
            if (msgBox.exec() != QMessageBox::Yes) return;
            std::error_code ec;
            fs::remove(path_copy, ec);
            if (!ec) refresh();
        });
        card_layout->addWidget(del_btn);

        ic_layout->addWidget(card);
    }
    ic_layout->addStretch();
}

}  // namespace omniface::ui
