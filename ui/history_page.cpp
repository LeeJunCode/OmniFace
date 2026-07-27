#include "omniface/ui/history_page.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace omniface::ui {

HistoryPage::HistoryPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background-color: #f5f5f5;");

    auto* hl = new QVBoxLayout(this);
    hl->setContentsMargins(12, 8, 12, 8);
    hl->setSpacing(8);

    auto* top = new QHBoxLayout;
    auto* back_btn = new QPushButton(QStringLiteral("\u2190 \u8fd4\u56de"));
    back_btn->setFixedWidth(100);
    connect(back_btn, &QPushButton::clicked, this, &HistoryPage::backRequested);
    top->addWidget(back_btn);
    top->addStretch();
    hl->addLayout(top);

    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    content_ = new QWidget;
    auto* hc_layout = new QVBoxLayout(content_);
    hc_layout->setContentsMargins(0, 0, 0, 0);
    hc_layout->setSpacing(8);
    scroll_->setWidget(content_);
    hl->addWidget(scroll_, 1);
}

void HistoryPage::refresh() {
    namespace fs = std::filesystem;
    std::string app_dir = QCoreApplication::applicationDirPath().toStdString();
    fs::path output_dir = fs::path(app_dir) / "data" / "output";

    if (content_->layout()) {
        QLayoutItem* item;
        while ((item = content_->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }
    auto* hc_layout = static_cast<QVBoxLayout*>(content_->layout());

    if (!fs::is_directory(output_dir)) {
        auto* empty_label = new QLabel(QStringLiteral("\u6682\u65e0\u5904\u7406\u8bb0\u5f55"));
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("color: #999; font-size: 14px; padding: 60px 0;");
        hc_layout->addWidget(empty_label);
        hc_layout->addStretch();
        return;
    }

    struct TaskInfo {
        std::string dir_path, video_name, time_str;
        int frame_count = 0;
    };
    std::vector<TaskInfo> tasks;

    for (const auto& entry : fs::directory_iterator(output_dir)) {
        if (!entry.is_directory()) continue;
        std::string dirname = entry.path().filename().string();
        if (dirname.size() < 16) continue;
        size_t last_underscore = dirname.rfind('_');
        if (last_underscore == std::string::npos || last_underscore < 9) continue;
        size_t date_underscore = dirname.rfind('_', last_underscore - 1);
        if (date_underscore == std::string::npos || date_underscore < 1) continue;

        std::string date_part = dirname.substr(date_underscore + 1, 8);
        std::string time_part = dirname.substr(last_underscore + 1, 6);

        TaskInfo info;
        info.dir_path = entry.path().string();
        info.video_name = dirname.substr(0, date_underscore);
        info.time_str = date_part.substr(0, 4) + "-" + date_part.substr(4, 2) + "-"
                        + date_part.substr(6, 2) + " "
                        + time_part.substr(0, 2) + ":" + time_part.substr(2, 2) + ":"
                        + time_part.substr(4, 2);

        for (const auto& f : fs::directory_iterator(entry.path())) {
            if (f.path().extension() == ".csv") {
                std::ifstream csv_file(f.path().string());
                if (csv_file.is_open()) {
                    std::string line;
                    while (std::getline(csv_file, line))
                        if (!line.empty()) ++info.frame_count;
                    if (info.frame_count > 0) --info.frame_count;
                }
            }
        }
        tasks.push_back(std::move(info));
    }

    if (tasks.empty()) {
        auto* empty_label = new QLabel(QStringLiteral("\u6682\u65e0\u5904\u7406\u8bb0\u5f55"));
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("color: #999; font-size: 14px; padding: 60px 0;");
        hc_layout->addWidget(empty_label);
        hc_layout->addStretch();
        return;
    }

    std::sort(tasks.begin(), tasks.end(),
              [](const TaskInfo& a, const TaskInfo& b) { return a.time_str > b.time_str; });

    for (const auto& t : tasks) {
        auto* card = new QFrame;
        card->setStyleSheet(
            "QFrame { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 6px; }"
            "QFrame:hover { border-color: #90caf9; background-color: #fafcff; }");
        auto* card_layout = new QHBoxLayout(card);
        card_layout->setContentsMargins(16, 12, 16, 12);
        card_layout->setSpacing(12);

        auto* info_widget = new QWidget;
        auto* info_layout = new QVBoxLayout(info_widget);
        info_layout->setContentsMargins(0, 0, 0, 0);
        info_layout->setSpacing(2);

        auto* name_label = new QLabel(QString::fromStdString(t.video_name));
        name_label->setStyleSheet("font-size: 14px; font-weight: bold; color: #333; border: none;");
        info_layout->addWidget(name_label);

        auto* meta_label = new QLabel(
            QStringLiteral("%1  |  %2 \u5e27")
                .arg(QString::fromStdString(t.time_str)).arg(t.frame_count));
        meta_label->setStyleSheet("font-size: 12px; color: #999; border: none;");
        info_layout->addWidget(meta_label);
        card_layout->addWidget(info_widget, 1);

        std::string dir_path = t.dir_path;
        auto* view_btn = new QPushButton(QStringLiteral("\u67e5\u770b\u8be6\u60c5"));
        view_btn->setFixedHeight(32);
        view_btn->setStyleSheet(
            "QPushButton { background-color: #1565c0; color: white; border: none; "
            "border-radius: 4px; padding: 6px 18px; font-size: 12px; font-weight: bold; }"
            "QPushButton:hover { background-color: #1976d2; }");
        connect(view_btn, &QPushButton::clicked, this, [this, dir_path]() {
            emit taskSelected(dir_path);
        });
        card_layout->addWidget(view_btn);

        auto* del_btn = new QPushButton(QStringLiteral("\u5220\u9664"));
        del_btn->setFixedHeight(32);
        del_btn->setStyleSheet(
            "QPushButton { background-color: #e53935; color: white; border: none; "
            "border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { background-color: #c62828; }");
        connect(del_btn, &QPushButton::clicked, this, [this, dir_path]() {
            emit taskDeleteRequested(dir_path);
        });
        card_layout->addWidget(del_btn);

        hc_layout->addWidget(card);
    }

    hc_layout->addStretch();
}

}  // namespace omniface::ui
