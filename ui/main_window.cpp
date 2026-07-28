#include "omniface/ui/main_window.h"

#include <filesystem>

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>

#include "omniface/ui/settings_dialog.h"
#include "omniface/ui/stylesheet.h"

namespace omniface::ui {

MainWindow::MainWindow(const PipelineConfig& config,
                         const std::string& config_path, QWidget* parent)
    : QMainWindow(parent), config_(config), config_path_(config_path) {
    setStyleSheet(AppStyleSheet());
    resize(1280, 800);
    setWindowTitle(QStringLiteral("OmniFace - \u5b9e\u65f6\u4eba\u8138\u68c0\u6d4b\u4e0e\u8bc6\u522b"));

    setupUI();
    setupMenus();

    auto* status_label = new QLabel(QStringLiteral("\u5c31\u7eea"));
    statusBar()->addWidget(status_label, 1);

    connect(process_page_, &ProcessPage::statusMessage, this, [status_label](const QString& msg) {
        status_label->setText(msg);
    });
    connect(process_page_, &ProcessPage::processingFinished, this, [this](const std::string&) {
        // completion is handled within ProcessPage
    });
    connect(process_page_, &ProcessPage::navigateToDetail, this, [this](const std::string& dir) {
        detail_page_->showTask(dir, false);
        central_stack_->setCurrentWidget(detail_page_);
    });
    connect(process_page_, &ProcessPage::processingStateChanged, this, [this](bool running) {
        if (saved_videos_action_)
            saved_videos_action_->setEnabled(!running);
        if (saved_images_action_)
            saved_images_action_->setEnabled(!running);
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    central_stack_ = new QStackedWidget;
    setCentralWidget(central_stack_);

    process_page_ = new ProcessPage(config_);
    central_stack_->addWidget(process_page_);

    detail_page_ = new DetailPage;
    connect(detail_page_, &DetailPage::backRequested, this, &MainWindow::onBackToProcess);
    connect(detail_page_, &DetailPage::backToHistory, this, [this]() {
        central_stack_->setCurrentWidget(history_page_);
    });
    connect(detail_page_, &DetailPage::deleteRequested, this, &MainWindow::onDeleteTask);
    central_stack_->addWidget(detail_page_);

    history_page_ = new HistoryPage;
    connect(history_page_, &HistoryPage::backRequested, this, &MainWindow::onBackToProcess);
    connect(history_page_, &HistoryPage::taskSelected, this, [this](const std::string& dir) {
        detail_page_->showTask(dir, true);
        central_stack_->setCurrentWidget(detail_page_);
    });
    connect(history_page_, &HistoryPage::taskDeleteRequested, this, &MainWindow::onDeleteTask);
    central_stack_->addWidget(history_page_);

    saved_videos_page_ = new SavedMediaPage(MediaType::Video);
    connect(saved_videos_page_, &SavedMediaPage::backRequested, this, &MainWindow::onBackToProcess);
    central_stack_->addWidget(saved_videos_page_);

    saved_images_page_ = new SavedMediaPage(MediaType::Image);
    connect(saved_images_page_, &SavedMediaPage::backRequested, this, &MainWindow::onBackToProcess);
    central_stack_->addWidget(saved_images_page_);

    central_stack_->setCurrentWidget(process_page_);
}

void MainWindow::setupMenus() {
    auto* file_menu = menuBar()->addMenu(QStringLiteral("\u6587\u4ef6(&F)"));
    file_menu->addAction(QStringLiteral("\u6253\u5f00\u89c6\u9891(&V)..."), process_page_, &ProcessPage::onOpenVideo);
    file_menu->addAction(QStringLiteral("\u6253\u5f00\u76ee\u6807\u4eba\u7269\u56fe\u50cf(&R)..."), process_page_, &ProcessPage::onOpenReference);

    auto* nav_menu = menuBar()->addMenu(QStringLiteral("\u5bfc\u822a(&N)"));
    nav_menu->addAction(QStringLiteral("\u8fd4\u56de\u4e3b\u754c\u9762(&M)"), this, &MainWindow::onBackToProcess);
    nav_menu->addSeparator();
    nav_menu->addAction(QStringLiteral("\u4efb\u52a1\u5386\u53f2(&H)..."), this, &MainWindow::onTaskHistory);
    saved_videos_action_ = nav_menu->addAction(QStringLiteral("\u67e5\u770b\u4e0a\u4f20\u7684\u89c6\u9891(&V)..."), this, [this]() {
        saved_videos_page_->refresh();
        central_stack_->setCurrentWidget(saved_videos_page_);
    });
    saved_images_action_ = nav_menu->addAction(QStringLiteral("\u67e5\u770b\u4e0a\u4f20\u7684\u56fe\u7247(&I)..."), this, [this]() {
        saved_images_page_->refresh();
        central_stack_->setCurrentWidget(saved_images_page_);
    });

    auto* tool_menu = menuBar()->addMenu(QStringLiteral("\u5de5\u5177(&T)"));
    tool_menu->addAction(QStringLiteral("\u53c2\u6570\u8bbe\u7f6e(&S)..."), this, &MainWindow::onSettings);

    auto* help_menu = menuBar()->addMenu(QStringLiteral("\u5e2e\u52a9(&H)"));
    help_menu->addAction(QStringLiteral("\u5173\u4e8e(&A)"), this, [this]() {
        QMessageBox about(this);
        about.setWindowTitle(QStringLiteral("\u5173\u4e8e OmniFace"));
        about.setText(QStringLiteral("OmniFace - \u5b9e\u65f6\u4eba\u8138\u68c0\u6d4b\u3001\u8ddf\u8e2a\u4e0e\u8bc6\u522b\n\n"
                                      "\u57fa\u4e8e SCRFD + ByteTrack + MobileFaceNet"));
        about.setIcon(QMessageBox::Information);
        about.setStyleSheet(AppStyleSheet());
        about.exec();
    });

    menuBar()->addAction(QStringLiteral("\u9000\u51fa(&X)"), this, &QWidget::close);
}

void MainWindow::onViewDetails() {
    // Handled by ProcessPage signal
}

void MainWindow::onTaskHistory() {
    history_page_->refresh();
    central_stack_->setCurrentWidget(history_page_);
}

void MainWindow::onBackToProcess() {
    central_stack_->setCurrentWidget(process_page_);
}

void MainWindow::onSettings() {
    SettingsDialog dlg(config_, this);
    if (dlg.exec() == QDialog::Accepted) {
        config_ = dlg.getConfig();
        process_page_->updateConfig(config_);
        spdlog::default_logger()->set_level(
            spdlog::level::from_str(config_.log.level));
        QMessageBox::information(this,
            QStringLiteral("\u751f\u6548"),
            QStringLiteral("\u53c2\u6570\u5df2\u66f4\u65b0\uff0c\u5f53\u524d\u5904\u7406\u4f1a\u8bdd\u751f\u6548\uff0c\u91cd\u65b0\u542f\u52a8\u540e\u6062\u590d\u9ed8\u8ba4\u503c\u3002"));
    }
}

void MainWindow::onDeleteTask(const std::string& dir_path) {
    if (dir_path.empty()) return;
    namespace fs = std::filesystem;
    fs::path out_dir(dir_path);
    if (!fs::is_directory(out_dir)) return;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("\u786e\u8ba4\u5220\u9664"));
    msgBox.setText(QStringLiteral("\u786e\u5b9a\u8981\u5220\u9664\u8be5\u4efb\u52a1\u53ca\u5176\u6240\u6709\u6587\u4ef6\u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setStyleSheet(ConfirmDialogStyleSheet());
    if (msgBox.exec() != QMessageBox::Yes) return;

    std::error_code ec;
    fs::remove_all(out_dir, ec);
    if (ec) {
        spdlog::warn("Failed to delete task directory: {}", ec.message());
        QMessageBox::warning(this, QStringLiteral("\u5220\u9664\u5931\u8d25"),
                             QStringLiteral("\u65e0\u6cd5\u5220\u9664\u4efb\u52a1\u76ee\u5f55\uff0c\u53ef\u80fd\u6587\u4ef6\u6b63\u5728\u88ab\u4f7f\u7528\u3002"));
        return;
    }

    if (central_stack_->currentWidget() == detail_page_)
        central_stack_->setCurrentWidget(process_page_);
    else
        onTaskHistory();
}

}  // namespace omniface::ui
