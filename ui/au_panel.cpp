#include "omniface/ui/au_panel.h"

#include <unordered_map>

#include <QPainter>
#include <QVBoxLayout>

namespace {

const std::unordered_map<std::string, QString> kAuDescriptions = {
    {"AU1", QStringLiteral("\u5185\u7709\u62ac")},
    {"AU2", QStringLiteral("\u5916\u7709\u62ac")},
    {"AU4", QStringLiteral("\u7709\u538b\u4f4e")},
    {"AU5", QStringLiteral("\u4e0a\u7751\u62ac")},
    {"AU6", QStringLiteral("\u8138\u988a\u62ac")},
    {"AU7", QStringLiteral("\u7751\u7d27\u7f29")},
    {"AU9", QStringLiteral("\u9f3b\u76b1")},
    {"AU10", QStringLiteral("\u4e0a\u5507\u62ac")},
    {"AU11", QStringLiteral("\u9f3b\u5507\u6c9f")},
    {"AU12", QStringLiteral("\u5634\u89d2\u626c")},
    {"AU13", QStringLiteral("\u8138\u988a\u9f13")},
    {"AU14", QStringLiteral("\u5634\u89d2\u7d27")},
    {"AU15", QStringLiteral("\u5634\u89d2\u6487")},
    {"AU16", QStringLiteral("\u4e0b\u5507\u538b")},
    {"AU17", QStringLiteral("\u4e0b\u5df4\u62ac")},
    {"AU18", QStringLiteral("\u5507\u805a\u62e2")},
    {"AU19", QStringLiteral("\u820c\u9732")},
    {"AU20", QStringLiteral("\u5507\u62c9\u4f38")},
    {"AU22", QStringLiteral("\u5507\u6f0f\u6597")},
    {"AU23", QStringLiteral("\u5507\u7d27\u7f29")},
    {"AU24", QStringLiteral("\u5507\u62bf\u7d27")},
    {"AU25", QStringLiteral("\u53cc\u5507\u5206")},
    {"AU26", QStringLiteral("\u4e0b\u988c\u5760")},
    {"AU27", QStringLiteral("\u5634\u62c9\u4f38")},
    {"AU32", QStringLiteral("\u54ac\u5507")},
    {"AU38", QStringLiteral("\u9f3b\u5b54\u6269")},
    {"AU39", QStringLiteral("\u9f3b\u5b54\u538b")},
    {"AUL1", QStringLiteral("\u5185\u7709\u62acL")},
    {"AUR1", QStringLiteral("\u5185\u7709\u62acR")},
    {"AUL2", QStringLiteral("\u5916\u7709\u62acL")},
    {"AUR2", QStringLiteral("\u5916\u7709\u62acR")},
    {"AUL4", QStringLiteral("\u7709\u538b\u4f4eL")},
    {"AUR4", QStringLiteral("\u7709\u538b\u4f4eR")},
    {"AUL6", QStringLiteral("\u8138\u988a\u62acL")},
    {"AUR6", QStringLiteral("\u8138\u988a\u62acR")},
    {"AUL10", QStringLiteral("\u4e0a\u5507\u62acL")},
    {"AUR10", QStringLiteral("\u4e0a\u5507\u62acR")},
    {"AUL12", QStringLiteral("\u9f3b\u5507\u6c9fL")},
    {"AUR12", QStringLiteral("\u9f3b\u5507\u6c9fR")},
    {"AUL14", QStringLiteral("\u5634\u89d2\u7d27L")},
    {"AUR14", QStringLiteral("\u5634\u89d2\u7d27R")},
};

}  // namespace

namespace omniface::ui {

AUPanel::AUPanel(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(240);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setStyleSheet(
        "QScrollArea { border: 1px solid #e0e5ec; border-radius: 6px; background: #ffffff; }"
        "QScrollBar:vertical { width: 6px; background: #fafafa; }"
        "QScrollBar::handle:vertical { background: #d0d0d0; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    content_label_ = new QLabel;
    content_label_->setWordWrap(true);
    content_label_->setTextFormat(Qt::RichText);
    content_label_->setAlignment(Qt::AlignTop);
    content_label_->setStyleSheet(
        "QLabel { background: #ffffff; border: none; padding: 0; }");
    scroll_area_->setWidget(content_label_);

    layout->addWidget(scroll_area_);
}

void AUPanel::setActionUnits(
    const std::vector<std::pair<std::string, double>>& action_units) {
    action_units_ = action_units;
    rebuildHtml();
}

void AUPanel::clear() {
    action_units_.clear();
    rebuildHtml();
}

void AUPanel::rebuildHtml() {
    if (action_units_.empty()) {
        content_label_->setText(QStringLiteral(
            "<div style='color:#999; font-size:12px; padding:8px;'>"
            "\u6682\u65e0 AU \u6570\u636e</div>"));
        return;
    }

    int active_count = 0;
    QStringList cards;

    for (const auto& [code, prob] : action_units_) {
        bool active = prob >= 0.5;
        if (active) ++active_count;

        auto desc_it = kAuDescriptions.find(code);
        QString desc = (desc_it != kAuDescriptions.end())
            ? desc_it->second : QString::fromStdString(code);

        QString bg = active ? "#e8f5e9" : "#fafafa";
        QString code_color = active ? "#2e7d32" : "#999";
        QString desc_color = active ? "#555" : "#bbb";
        QString prob_color = active ? "#2e7d32" : "#ccc";

        cards << QStringLiteral(
            "<div style='display:inline-block; width:32%%; margin:0.5px 0.6%%; "
            "padding:1px 3px; background:%1; border-radius:2px; font-size:10px;'>"
            "<span style='color:%2; font-weight:bold;'>%3</span>"
            "<span style='color:%4;'>%5</span>"
            "<span style='float:right; color:%6;'>%7</span>"
            "</div>")
            .arg(bg)
            .arg(code_color)
            .arg(QString::fromStdString(code))
            .arg(desc_color)
            .arg(desc)
            .arg(prob_color)
            .arg(QString::number(prob, 'f', 2));
    }

    QString html;
    html += QStringLiteral(
        "<div style='font-size:10px; line-height:1.4; color:#333; padding:4px 6px;'>"
        "<div style='padding:2px 0; margin-bottom:2px; font-size:11px;'>"
        "<b style='color:#333;'>\u52a8\u4f5c\u5355\u5143</b> "
        "<span style='color:#888;'>%1 / 41 \u6d3b\u8dc3</span>"
        "</div>"
        "<div style='line-height:1.2;'>").arg(active_count);
    for (const auto& card : cards)
        html += card;
    html += "</div></div>";

    content_label_->setText(html);
}

}  // namespace omniface::ui
