#include "omniface/ui/pages/image_detail_page.h"

#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

#include "omniface/db/image_repo.h"
#include "omniface/ui/theme.h"

namespace omniface {
namespace ui {

ImageDetailPage::ImageDetailPage(std::shared_ptr<db::ImageRepo> repo, QWidget* parent)
    : QWidget(parent), repo_(std::move(repo)) {
    SetupUI();
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged,
            this, &ImageDetailPage::ReapplyTheme);
}

void ImageDetailPage::SetupUI() {
    auto& tm = ThemeManager::Instance();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 8, 16, 16);
    root->setSpacing(8);

    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QStringLiteral("← 返回"), this);
    back_btn_ = back_btn;
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; }"
        "QPushButton:hover { color: %2; }").arg(tm.Primary(), tm.AccentBlue()));
    connect(back_btn, &QPushButton::clicked, this, &ImageDetailPage::BackRequested);
    top_bar->addWidget(back_btn);
    top_bar->addStretch();

    auto* edit_btn = new QPushButton(QStringLiteral("编辑"), this);
    edit_btn_ = edit_btn;
    edit_btn->setCursor(Qt::PointingHandCursor);
    edit_btn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background: %1; }").arg(tm.HoverBg(), tm.TextPrimary()));
    connect(edit_btn, &QPushButton::clicked, this, [this]() {
        if (image_id_ > 0) emit EditRequested(image_id_);
    });
    top_bar->addWidget(edit_btn);
    root->addLayout(top_bar);

    auto* main_area = new QHBoxLayout();
    main_area->setSpacing(12);

    image_display_ = new QLabel(this);
    image_display_->setMinimumSize(320, 320);
    image_display_->setAlignment(Qt::AlignCenter);
    image_display_->setStyleSheet(QString(
        "QLabel { background: %1; border: 1px solid %2; }").arg(tm.DeepBg(), tm.Border()));
    main_area->addWidget(image_display_, 2);

    auto* right = new QVBoxLayout();
    right->setSpacing(8);

    auto* info_panel = new QGroupBox(QStringLiteral("图片信息"), this);
    info_panel_ = info_panel;
    info_panel->setFixedWidth(260);
    info_panel->setStyleSheet(QString(
        "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
        "  border-radius: 4px; margin-top: 8px; padding-top: 14px; background: %3; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }")
        .arg(tm.TextPrimary(), tm.Border(), tm.AltRow()));
    auto* info_layout = new QFormLayout(info_panel);
    info_layout->setSpacing(8);
    auto addField = [&](const QString& label, QLabel*& value) {
        value = new QLabel(QStringLiteral("—"), info_panel);
        value->setWordWrap(true);
        value->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.TextMuted()));
        info_layout->addRow(label, value);
    };
    addField(QStringLiteral("名称:"), name_label_);
    addField(QStringLiteral("描述:"), description_label_);
    addField(QStringLiteral("备注:"), notes_label_);
    addField(QStringLiteral("人脸置信度:"), confidence_label_);
    addField(QStringLiteral("保存路径:"), saved_path_label_);
    addField(QStringLiteral("上传时间:"), created_at_label_);
    right->addWidget(info_panel);

    embedding_panel_ = new QWidget(this);
    embedding_panel_->setVisible(false);
    auto* emb_layout = new QVBoxLayout(embedding_panel_);
    emb_layout->setContentsMargins(0, 0, 0, 0);
    auto* emb_title = new QLabel(QStringLiteral("512维特征向量"), this);
    emb_title->setStyleSheet(QString("QLabel { color: %1; font-weight: bold; }").arg(tm.TextMuted()));
    emb_layout->addWidget(emb_title);
    auto* emb_text = new QLabel(this);
    emb_text->setWordWrap(true);
    emb_text->setStyleSheet(QString(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 8px; font-size: 10px; font-family: monospace; }")
        .arg(tm.DeepBg(), tm.TextDim(), tm.Border()));
    auto* emb_scroll = new QScrollArea(this);
    emb_scroll->setWidget(emb_text);
    emb_scroll->setWidgetResizable(true);
    emb_scroll->setStyleSheet("QScrollArea { border: none; }");
    emb_layout->addWidget(emb_scroll);
    right->addWidget(embedding_panel_);

    auto* toggle_emb = new QPushButton(QStringLiteral("展开特征向量 ▼"), this);
    toggle_emb_btn_ = toggle_emb;
    toggle_emb->setCursor(Qt::PointingHandCursor);
    toggle_emb->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 12px; font-size: 11px; }"
        "QPushButton:hover { background: %1; }").arg(tm.HoverBg(), tm.TextMuted()));
    connect(toggle_emb, &QPushButton::clicked, this, [emb_text, toggle_emb, this]() {
        bool vis = !embedding_panel_->isVisible();
        embedding_panel_->setVisible(vis);
        toggle_emb->setText(vis ? QStringLiteral("折叠特征向量 ▲") : QStringLiteral("展开特征向量 ▼"));
        if (vis && emb_text->text().isEmpty() && !original_image_.isNull()) {
            auto rec = repo_->FindById(image_id_);
            if (rec) {
                QStringList vals;
                for (int i = 0; i < 512; ++i) vals << QStringLiteral("%1").arg(rec->embedding[i], 0, 'f', 6);
                emb_text->setText(vals.join(QStringLiteral(", ")));
            }
        }
    });
    right->addWidget(toggle_emb);

    right->addStretch();
    main_area->addLayout(right);
    root->addLayout(main_area, 1);
}

void ImageDetailPage::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();
    back_btn_->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; }"
        "QPushButton:hover { color: %2; }").arg(tm.Primary(), tm.AccentBlue()));
    edit_btn_->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background: %1; }").arg(tm.HoverBg(), tm.TextPrimary()));
    image_display_->setStyleSheet(QString(
        "QLabel { background: %1; border: 1px solid %2; }").arg(tm.DeepBg(), tm.Border()));
    if (info_panel_) {
        info_panel_->setStyleSheet(QString(
            "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2;"
            "  border-radius: 4px; margin-top: 8px; padding-top: 14px; background: %3; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }")
            .arg(tm.TextPrimary(), tm.Border(), tm.AltRow()));
    }
    auto reLabel = [&](QLabel* lbl) {
        if (lbl) lbl->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.TextMuted()));
    };
    reLabel(name_label_);
    reLabel(description_label_);
    reLabel(notes_label_);
    reLabel(confidence_label_);
    reLabel(saved_path_label_);
    reLabel(created_at_label_);
    toggle_emb_btn_->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %2; border: none;"
        "  border-radius: 4px; padding: 6px 12px; font-size: 11px; }"
        "QPushButton:hover { background: %1; }").arg(tm.HoverBg(), tm.TextMuted()));
    if (!original_image_.isNull()) {
        RenderAnnotatedPixmap();
    }
}

void ImageDetailPage::LoadImage(int64_t image_id) {
    image_id_ = image_id;
    auto rec = repo_->FindById(image_id);
    if (rec) {
        Populate(*rec);
        original_image_ = QImage(QString::fromStdString(rec->saved_path));
        if (!original_image_.isNull()) {
            bbox_x1_ = rec->bbox_x1; bbox_y1_ = rec->bbox_y1;
            bbox_x2_ = rec->bbox_x2; bbox_y2_ = rec->bbox_y2;
            for (int i = 0; i < 10; ++i) kp_[i] = rec->keypoints[i];
            has_bbox_ = (bbox_x2_ > bbox_x1_ && bbox_y2_ > bbox_y1_);
            RenderAnnotatedPixmap();
        }
    }
}

void ImageDetailPage::RenderAnnotatedPixmap() {
    if (original_image_.isNull() || image_display_->size().isEmpty()) return;

    QSize targetSize = image_display_->size();
    QImage scaled = original_image_.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    double sx = static_cast<double>(scaled.width()) / original_image_.width();
    double sy = static_cast<double>(scaled.height()) / original_image_.height();

    QPixmap pm = QPixmap::fromImage(scaled);
    if (has_bbox_) {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);

        QRectF box(bbox_x1_ * sx, bbox_y1_ * sy,
                   (bbox_x2_ - bbox_x1_) * sx, (bbox_y2_ - bbox_y1_) * sy);
        p.setPen(QPen(QColor(ThemeManager::Instance().Primary()), 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(box);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(ThemeManager::Instance().Primary()));
        for (int k = 0; k < 5; ++k) {
            float kx = kp_[k * 2] * sx;
            float ky = kp_[k * 2 + 1] * sy;
            if (kx > 0 && ky > 0)
                p.drawEllipse(QPointF(kx, ky), 3, 3);
        }
        p.end();
    }
    image_display_->setPixmap(pm);
}

void ImageDetailPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!original_image_.isNull())
        RenderAnnotatedPixmap();
}

void ImageDetailPage::Populate(const db::ImageRecord& rec) {
    name_label_->setText(QString::fromStdString(rec.name));
    description_label_->setText(rec.description.empty() ? QStringLiteral("—") : QString::fromStdString(rec.description));
    notes_label_->setText(rec.notes.empty() ? QStringLiteral("—") : QString::fromStdString(rec.notes));
    confidence_label_->setText(QStringLiteral("%1%").arg(rec.face_confidence * 100.0, 0, 'f', 1));
    saved_path_label_->setText(QString::fromStdString(rec.saved_path));
    if (!rec.created_at.empty()) {
        QString ts = QString::fromStdString(rec.created_at);
        ts.replace(QStringLiteral("T"), QStringLiteral(" "));
        int dot = ts.lastIndexOf(QStringLiteral("."));
        if (dot > 0) ts = ts.left(dot);
        created_at_label_->setText(ts);
    } else {
        created_at_label_->setText(QStringLiteral("—"));
    }
}

}  // namespace ui
}  // namespace omniface
