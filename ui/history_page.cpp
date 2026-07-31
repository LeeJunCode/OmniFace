#include "omniface/ui/history_page.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <QApplication>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>

#include "omniface/ui/stylesheet.h"

namespace omniface::ui {

HistoryPage::HistoryPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background-color: #f5f5f5;");

    auto* hl = new QVBoxLayout(this);
    hl->setContentsMargins(12, 8, 12, 8);
    hl->setSpacing(8);

    auto* top = new QHBoxLayout;
    auto* back_btn = new QPushButton(QStringLiteral("\u2190 \u8fd4\u56de"));
    back_btn->setFixedHeight(28);
    connect(back_btn, &QPushButton::clicked, this, &HistoryPage::backRequested);
    top->addWidget(back_btn);
    top->addStretch();
    hl->addLayout(top);

    // 批量操作栏（无选中项时隐藏）
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
    connect(select_all_cb_, &QCheckBox::clicked, this, &HistoryPage::onSelectAll);
    batch_layout->addWidget(select_all_cb_);

    batch_del_btn_ = new QPushButton(QStringLiteral("\u6279\u91cf\u5220\u9664"));
    batch_del_btn_->setFixedHeight(30);
    batch_del_btn_->setStyleSheet(
        "QPushButton { background-color: #e53935; color: white; border: none; "
        "border-radius: 4px; padding: 4px 16px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #c62828; }"
        "QPushButton:disabled { background-color: #ccc; }");
    connect(batch_del_btn_, &QPushButton::clicked, this, &HistoryPage::onBatchDelete);
    batch_layout->addWidget(batch_del_btn_);
    batch_layout->addStretch();

    batch_bar_->hide();
    hl->addWidget(batch_bar_);

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

void HistoryPage::onSelectAll() {
    bool checked = select_all_cb_->isChecked();
    if (checked) {
        selected_.clear();
        auto* hc_layout = static_cast<QVBoxLayout*>(content_->layout());
        for (int i = 0; i < hc_layout->count(); ++i) {
            auto* frame = qobject_cast<QFrame*>(hc_layout->itemAt(i)->widget());
            if (!frame) continue;
            auto* cb = frame->findChild<QCheckBox*>("taskCb");
            if (cb) {
                cb->setChecked(true);
                selected_.insert(cb->property("dir_path").toString().toStdString());
            }
        }
    } else {
        auto* hc_layout = static_cast<QVBoxLayout*>(content_->layout());
        for (int i = 0; i < hc_layout->count(); ++i) {
            auto* frame = qobject_cast<QFrame*>(hc_layout->itemAt(i)->widget());
            if (!frame) continue;
            auto* cb = frame->findChild<QCheckBox*>("taskCb");
            if (cb) cb->setChecked(false);
        }
        selected_.clear();
    }
    updateBatchButtons();
}

void HistoryPage::onBatchDelete() {
    if (selected_.empty()) return;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("\u786e\u8ba4\u6279\u91cf\u5220\u9664"));
    msgBox.setText(QStringLiteral("\u786e\u5b9a\u8981\u5220\u9664\u6240\u9009 %1 \u4e2a\u4efb\u52a1\u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002").arg(static_cast<int>(selected_.size())));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setStyleSheet(ConfirmDialogStyleSheet());
    if (msgBox.exec() != QMessageBox::Yes) return;

    namespace fs = std::filesystem;
    for (const auto& dir_path : selected_) {
        std::error_code ec;
        fs::remove_all(dir_path, ec);
    }
    selected_.clear();
    refresh();
}

void HistoryPage::updateBatchButtons() {
    int n = static_cast<int>(selected_.size());
    batch_del_btn_->setEnabled(n > 0);
    if (n > 0)
        batch_del_btn_->setText(QStringLiteral("\u6279\u91cf\u5220\u9664 (%1)").arg(n));
    else
        batch_del_btn_->setText(QStringLiteral("\u6279\u91cf\u5220\u9664"));
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

    selected_.clear();
    total_items_ = 0;

    if (!fs::is_directory(output_dir)) {
        auto* empty_label = new QLabel(QStringLiteral("\u6682\u65e0\u5904\u7406\u8bb0\u5f55"));
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("color: #999; font-size: 14px; padding: 60px 0;");
        hc_layout->addWidget(empty_label);
        hc_layout->addStretch();
        batch_bar_->hide();
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
        batch_bar_->hide();
        return;
    }

    total_items_ = static_cast<int>(tasks.size());
    batch_bar_->show();
    select_all_cb_->setChecked(false);
    updateBatchButtons();

    std::sort(tasks.begin(), tasks.end(),
              [](const TaskInfo& a, const TaskInfo& b) { return a.time_str > b.time_str; });

    for (const auto& t : tasks) {
        auto* card = new QFrame;
        card->setStyleSheet(
            "QFrame { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 6px; }"
            "QFrame:hover { border-color: #90caf9; background-color: #fafcff; }");
        card->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(card, &QFrame::customContextMenuRequested, this,
                [this, dir_path = t.dir_path](const QPoint& pos) {
                    QMenu menu;
                    auto* open_dir_action = menu.addAction(
                        QStringLiteral("\u67e5\u770b\u6587\u4ef6\u6240\u5728\u76ee\u5f55"));
                    connect(open_dir_action, &QAction::triggered, this, [dir_path]() {
                        QDesktopServices::openUrl(
                            QUrl::fromLocalFile(QString::fromStdString(dir_path)));
                    });
                    menu.exec(QCursor::pos());
                });
        auto* card_layout = new QHBoxLayout(card);
        card_layout->setContentsMargins(12, 12, 16, 12);
        card_layout->setSpacing(10);

        // 复选框
        auto* cb = new QCheckBox;
        cb->setObjectName("taskCb");
        cb->setProperty("dir_path", QString::fromStdString(t.dir_path));
        cb->setStyleSheet(
            "QCheckBox { border: none; spacing: 0px; }"
            "QCheckBox::indicator { width: 18px; height: 18px; border: 2px solid #bdbdbd; "
            "border-radius: 3px; background-color: #fafafa; }"
            "QCheckBox::indicator:hover { border-color: #1565c0; }"
            "QCheckBox::indicator:checked { background-color: #1565c0; "
            "border-color: #1565c0; }");
        connect(cb, &QCheckBox::toggled, this, [this, dir_path = t.dir_path](bool checked) {
            if (checked)
                selected_.insert(dir_path);
            else
                selected_.erase(dir_path);
            updateBatchButtons();
        });
        card_layout->addWidget(cb);

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
