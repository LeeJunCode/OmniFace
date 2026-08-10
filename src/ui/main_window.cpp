#include "omniface/ui/main_window.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextStream>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QStatusBar>
#include <QStyleFactory>

#include "omniface/db/database.h"
#include "omniface/db/video_repo.h"
#include "omniface/db/image_repo.h"
#include "omniface/db/task_repo.h"
#include "omniface/ui/pages/home_page.h"
#include "omniface/ui/pages/image_detail_page.h"
#include "omniface/ui/pages/image_management_page.h"
#include "omniface/ui/pages/processing_page.h"
#include "omniface/ui/pages/task_detail_page.h"
#include "omniface/ui/pages/task_history_page.h"
#include "omniface/ui/pages/video_detail_page.h"
#include "omniface/ui/pages/video_management_page.h"
#include "omniface/pipeline/framework/config.h"
#include "omniface/types.h"
#include "omniface/ui/dialogs/about_dialog.h"
#include "omniface/ui/dialogs/csv_reference_dialog.h"
#include "omniface/ui/dialogs/edit_image_dialog.h"
#include "omniface/ui/dialogs/edit_task_dialog.h"
#include "omniface/ui/dialogs/edit_video_dialog.h"
#include "omniface/ui/dialogs/settings_dialog.h"
#include "omniface/ui/theme.h"

namespace omniface {
namespace ui {

namespace {

using IconPainter = std::function<void(QPainter&, const QRect&)>;

QIcon MakeIcon(const IconPainter& painter, const QColor& color) {
    QPixmap px(24, 24);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 2.0));
    painter(p, px.rect());
    p.end();
    return QIcon(px);
}

// Icon painters for sidebar navigation buttons (indices 0–4)
const IconPainter kIconPainters[] = {
    // 0: Home
    [](QPainter& p, const QRect& r) {
        QPoint c = r.center();
        int x = r.left() + 3, y = r.top() + 3;
        int w = r.width() - 6, h = r.height() - 5;
        QPolygon roof;
        roof << QPoint(r.center().x(), y) << QPoint(x, y + h / 2) << QPoint(x + w, y + h / 2);
        p.drawPolygon(roof);
        p.drawRect(x + 2, y + h / 2, w - 4, h / 2);
        p.drawRect(c.x() - 2, c.y() + 3, 4, h / 2 - 3);
    },
    // 1: Process
    [](QPainter& p, const QRect& r) {
        QPoint c = r.center();
        p.drawEllipse(c, 3, 3);
        p.drawEllipse(c, 8, 8);
        for (int i = 0; i < 8; ++i) {
            double a = i * 3.14159 / 4;
            int x1 = c.x() + static_cast<int>(7  * cos(a));
            int y1 = c.y() + static_cast<int>(7  * sin(a));
            int x2 = c.x() + static_cast<int>(10 * cos(a));
            int y2 = c.y() + static_cast<int>(10 * sin(a));
            p.drawLine(x1, y1, x2, y2);
        }
    },
    // 2: Video
    [](QPainter& p, const QRect& r) {
        p.drawRect(r.adjusted(3, 4, -3, -4));
        int cx = r.center().x() + 2;
        int cy = r.center().y();
        QPolygon tri;
        tri << QPoint(cx - 3, cy - 4) << QPoint(cx - 3, cy + 4) << QPoint(cx + 3, cy);
        p.drawPolygon(tri);
    },
    // 3: Image
    [](QPainter& p, const QRect& r) {
        p.drawRect(r.adjusted(3, 3, -3, -3));
        p.drawEllipse(QPoint(r.right() - 6, r.top() + 6), 3, 3);
        p.drawLine(r.left() + 3, r.bottom() - 4, r.left() + 9, r.bottom() - 7);
        p.drawLine(r.left() + 9, r.bottom() - 7, r.right() - 3, r.bottom() - 4);
    },
    // 4: History
    [](QPainter& p, const QRect& r) {
        QPoint c = r.center();
        p.drawEllipse(c, 9, 9);
        p.drawLine(c, QPoint(c.x(), c.y() - 5));
        p.drawLine(c, QPoint(c.x() + 5, c.y()));
    },
};

QString DataDir() {
    QString dir = QApplication::applicationDirPath() + QStringLiteral("/data");
    QDir().mkpath(dir);
    return dir;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    ApplyTheme();

    setWindowTitle(QStringLiteral("OmniFace"));
    resize(1280, 800);
    setMinimumSize(960, 600);

    // Initialize database
    database_ = std::make_unique<db::Database>(DataDir() + QStringLiteral("/omniface.db"));

    SetupMenuBar();
    SetupStatusBar();

    auto* central = new QWidget(this);
    auto* hlayout = new QHBoxLayout(central);
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(0);

    SetupSidebar();
    hlayout->addWidget(sidebar_);

    separator_ = new QFrame(this);
    separator_->setFrameShape(QFrame::VLine);
    separator_->setStyleSheet(
        QString("QFrame { color: %1; }").arg(ThemeManager::Instance().Border()));
    hlayout->addWidget(separator_);

    SetupContentArea();
    hlayout->addWidget(content_stack_, 1);

    setCentralWidget(central);

    nav_buttons_->button(0)->setChecked(true);
    OnPageChanged(0);
}

MainWindow::~MainWindow() = default;

void MainWindow::ApplyTheme() {
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setPalette(ThemeManager::Instance().CreatePalette());
}

void MainWindow::ReapplyWidgetStyles() {
    auto& tm = ThemeManager::Instance();

    if (separator_)
        separator_->setStyleSheet(
            QString("QFrame { color: %1; }").arg(tm.Border()));

    sidebar_->setStyleSheet(
        QString("QWidget { background: %1; }").arg(tm.SidebarBg()));

    // Regenerate sidebar icons with current theme color
    QColor iconColor(tm.TextPrimary());
    for (int i = 0; i < 5; ++i) {
        if (auto* btn = nav_buttons_->button(i)) {
            btn->setIcon(MakeIcon(kIconPainters[i], iconColor));
        }
    }

    for (auto* btn : nav_buttons_->buttons()) {
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  border: none;"
            "  background: transparent;"
            "  padding: 0px;"
            "}"
            "QPushButton:hover {"
            "  background: %1;"
            "}"
            "QPushButton:checked {"
            "  background: %2;"
            "  border-left: 2px solid %3;"
            "}").arg(tm.HoverBg(), tm.SelectedBg(), tm.Primary()));
    }

    status_indicator_->setStyleSheet(
        QString("background: %1; border-radius: 6px;").arg(tm.TextDim()));

    statusBar()->setStyleSheet(
        QString("QStatusBar { background: %1; border-top: 1px solid %2; }"
                "QStatusBar::item { border: none; }")
            .arg(tm.DeepBg(), tm.Border()));

    home_page_->ReapplyTheme();
    process_page_->ReapplyTheme();
    video_page_->ReapplyTheme();
    image_page_->ReapplyTheme();
    history_page_->ReapplyTheme();
    video_detail_page_->ReapplyTheme();
    image_detail_page_->ReapplyTheme();
    task_detail_page_->ReapplyTheme();
}

void MainWindow::SetupMenuBar() {
    auto* file_menu = menuBar()->addMenu(QStringLiteral("文件"));

    auto* import_video = file_menu->addAction(QStringLiteral("导入视频..."));
    import_video->setShortcut(QKeySequence(QStringLiteral("Ctrl+V")));
    connect(import_video, &QAction::triggered, this, [this]() {
        video_page_->TriggerImport();
    });

    auto* import_image = file_menu->addAction(QStringLiteral("导入图片..."));
    import_image->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
    connect(import_image, &QAction::triggered, this, [this]() {
        image_page_->TriggerImport();
    });

    file_menu->addSeparator();

    auto* exit_action = file_menu->addAction(QStringLiteral("退出"));
    exit_action->setShortcut(QKeySequence(QStringLiteral("Alt+F4")));
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    auto* settings_menu = menuBar()->addMenu(QStringLiteral("设置"));
    auto* settings_action = settings_menu->addAction(QStringLiteral("参数设置..."));
    connect(settings_action, &QAction::triggered, this, [this]() {
        if (process_page_->IsProcessing()) {
            auto result = QMessageBox::warning(this,
                QStringLiteral("处理进行中"),
                QStringLiteral("处理任务正在进行中，修改参数可能导致结果不一致。\n\n确定继续？"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (result != QMessageBox::Yes) return;
        }
        SettingsDialog dlg(this);
        dlg.SetConfig(process_page_->GetConfig());
        connect(&dlg, &SettingsDialog::ConfigChanged, this, [this](const PipelineConfig& cfg) {
            process_page_->SetConfig(cfg);
            QString configPath = QApplication::applicationDirPath() + QStringLiteral("/config.yaml");
            try {
                SavePipelineConfig(configPath.toStdString(), cfg);
            } catch (const std::exception&) {
                // Silently ignore save failures
            }
        });
        dlg.exec();
    });

    settings_menu->addSeparator();

    auto* theme_menu = settings_menu->addMenu(QStringLiteral("主题"));
    auto* dark_action = theme_menu->addAction(QStringLiteral("暗色模式"));
    dark_action->setCheckable(true);
    auto* light_action = theme_menu->addAction(QStringLiteral("亮色模式"));
    light_action->setCheckable(true);
    light_action->setChecked(true);

    connect(dark_action, &QAction::triggered, this, [this, dark_action, light_action]() {
        ThemeManager::Instance().SetTheme(Theme::Dark);
        dark_action->setChecked(true);
        light_action->setChecked(false);
        ApplyTheme();
        ReapplyWidgetStyles();
    });
    connect(light_action, &QAction::triggered, this, [this, dark_action, light_action]() {
        ThemeManager::Instance().SetTheme(Theme::Light);
        light_action->setChecked(true);
        dark_action->setChecked(false);
        ApplyTheme();
        ReapplyWidgetStyles();
    });

    auto* help_menu = menuBar()->addMenu(QStringLiteral("帮助"));
    auto* csv_ref_action = help_menu->addAction(QStringLiteral("CSV 数据参考..."));
    connect(csv_ref_action, &QAction::triggered, this, [this]() {
        CsvReferenceDialog dlg(this);
        dlg.exec();
    });
    auto* about_action = help_menu->addAction(QStringLiteral("关于"));
    connect(about_action, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
}

QPushButton* MainWindow::CreateSidebarButton(const QIcon& icon, const QString& tooltip) {
    auto* btn = new QPushButton(this);
    btn->setIcon(icon);
    btn->setIconSize(QSize(22, 22));
    btn->setToolTip(tooltip);
    btn->setCheckable(true);
    btn->setFixedSize(48, 48);
    btn->setCursor(Qt::PointingHandCursor);
    auto& tm = ThemeManager::Instance();
    btn->setStyleSheet(QString(
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background: %1;"
        "}"
        "QPushButton:checked {"
        "  background: %2;"
        "  border-left: 2px solid %3;"
        "}").arg(tm.HoverBg(), tm.SelectedBg(), tm.Primary()));
    return btn;
}

void MainWindow::SetupSidebar() {
    sidebar_ = new QWidget(this);
    sidebar_->setFixedWidth(48);
    sidebar_->setStyleSheet(
        QString("QWidget { background: %1; }").arg(ThemeManager::Instance().SidebarBg()));

    sidebar_layout_ = new QVBoxLayout(sidebar_);
    sidebar_layout_->setContentsMargins(0, 4, 0, 4);
    sidebar_layout_->setSpacing(2);

    nav_buttons_ = new QButtonGroup(this);
    nav_buttons_->setExclusive(true);

    QColor iconColor(ThemeManager::Instance().TextPrimary());

    const QString tips[] = {
        QStringLiteral("主界面"),
        QStringLiteral("处理"),
        QStringLiteral("视频管理"),
        QStringLiteral("图片管理"),
        QStringLiteral("任务历史"),
    };

    for (int i = 0; i < 5; ++i) {
        auto* btn = CreateSidebarButton(MakeIcon(kIconPainters[i], iconColor), tips[i]);
        nav_buttons_->addButton(btn, i);
        sidebar_layout_->addWidget(btn, 0, Qt::AlignHCenter);
    }

    sidebar_layout_->addStretch();

    status_indicator_ = new QWidget(this);
    status_indicator_->setFixedSize(12, 12);
    status_indicator_->setStyleSheet(
        QString("background: %1; border-radius: 6px;").arg(ThemeManager::Instance().TextDim()));
    sidebar_layout_->addWidget(status_indicator_, 0, Qt::AlignHCenter);
    sidebar_layout_->addSpacing(8);

    connect(nav_buttons_, &QButtonGroup::idClicked,
            this, &MainWindow::OnPageChanged);
}

void MainWindow::SetupContentArea() {
    content_stack_ = new QStackedWidget(this);

    auto video_repo = std::make_shared<db::VideoRepo>(database_->Get());
    auto image_repo = std::make_shared<db::ImageRepo>(database_->Get());
    auto task_repo  = std::make_shared<db::TaskRepo>(database_->Get());

    home_page_    = new HomePage(video_repo, image_repo, task_repo, this);
    process_page_ = new ProcessingPage(video_repo, image_repo, this);
    video_page_   = new VideoManagementPage(video_repo, this);
    image_page_   = new ImageManagementPage(image_repo, this);
    video_page_->SetProcessingPage(process_page_);
    image_page_->SetProcessingPage(process_page_);
    history_page_ = new TaskHistoryPage(task_repo, video_repo, image_repo, this);

    // Load persisted config if available
    {
        QString configPath = QApplication::applicationDirPath() + QStringLiteral("/config.yaml");
        if (QFile::exists(configPath)) {
            try {
                auto cfg = LoadPipelineConfig(configPath.toStdString());
                process_page_->SetConfig(cfg);
            } catch (const std::exception&) {
                // Use defaults if config file is corrupt
            }
        }
    }

    content_stack_->addWidget(home_page_);     // index 0
    content_stack_->addWidget(process_page_);  // index 1
    content_stack_->addWidget(video_page_);    // index 2
    content_stack_->addWidget(image_page_);    // index 3
    content_stack_->addWidget(history_page_);  // index 4

    // Wire home page navigation
    connect(home_page_, &HomePage::NavigateToProcessing, this, [this] {
        nav_buttons_->button(1)->setChecked(true);
        OnPageChanged(1);
    });
    connect(home_page_, &HomePage::NavigateToVideos, this, [this] {
        nav_buttons_->button(2)->setChecked(true);
        OnPageChanged(2);
    });
    connect(home_page_, &HomePage::NavigateToImages, this, [this] {
        nav_buttons_->button(3)->setChecked(true);
        OnPageChanged(3);
    });
    connect(home_page_, &HomePage::NavigateToHistory, this, [this] {
        nav_buttons_->button(4)->setChecked(true);
        OnPageChanged(4);
    });

    // Wire page-to-page signals
    connect(video_page_, &VideoManagementPage::VideoSelectedForProcessing,
            process_page_, &ProcessingPage::SetSelectedVideo);
    connect(image_page_, &ImageManagementPage::ImageSelectedForProcessing,
            this, [this, image_repo](int64_t image_id, const QString& name) {
        auto rec = image_repo->FindById(image_id);
        if (rec) {
            FaceEmbedding emb;
            emb.features = rec->embedding;
            emb.valid = true;
            process_page_->SetSelectedImage(image_id, name, emb);
        }
    });

    // Wire edit signals
    connect(video_page_, &VideoManagementPage::EditRequested,
            this, [this, video_repo](int64_t video_id) {
        EditVideoDialog dlg(video_repo.get(), video_id, this);
        if (dlg.exec() == QDialog::Accepted)
            video_page_->Refresh();
    });
    connect(image_page_, &ImageManagementPage::EditRequested,
            this, [this, image_repo](int64_t image_id) {
        EditImageDialog dlg(image_repo.get(), image_id, this);
        if (dlg.exec() == QDialog::Accepted)
            image_page_->Refresh();
    });

    // ── Detail pages ──────────────────────────────────────────

    video_detail_page_ = new VideoDetailPage(video_repo, this);
    image_detail_page_ = new ImageDetailPage(image_repo, this);
    task_detail_page_  = new TaskDetailPage(task_repo, video_repo, image_repo, this);

    content_stack_->addWidget(video_detail_page_);   // index 5
    content_stack_->addWidget(image_detail_page_);    // index 6
    content_stack_->addWidget(task_detail_page_);     // index 7

    // View details → show detail page
    connect(video_page_, &VideoManagementPage::ViewDetailsRequested,
            this, [this](int64_t video_id) {
        video_detail_page_->LoadVideo(video_id);
        ShowDetailPage(video_detail_page_);
    });
    connect(image_page_, &ImageManagementPage::ViewDetailsRequested,
            this, [this](int64_t image_id) {
        image_detail_page_->LoadImage(image_id);
        ShowDetailPage(image_detail_page_);
    });
    connect(history_page_, &TaskHistoryPage::ViewTaskDetailRequested,
            this, [this](int64_t task_id) {
        task_detail_page_->LoadTask(task_id);
        ShowDetailPage(task_detail_page_);
    });

    // Back from detail
    connect(video_detail_page_, &VideoDetailPage::BackRequested,
            this, &MainWindow::OnBackFromDetail);
    connect(image_detail_page_, &ImageDetailPage::BackRequested,
            this, &MainWindow::OnBackFromDetail);
    connect(task_detail_page_, &TaskDetailPage::BackRequested,
            this, &MainWindow::OnBackFromDetail);

    // Edit from detail pages
    connect(video_detail_page_, &VideoDetailPage::EditRequested,
            this, [this, video_repo](int64_t video_id) {
        EditVideoDialog dlg(video_repo.get(), video_id, this);
        if (dlg.exec() == QDialog::Accepted)
            video_detail_page_->LoadVideo(video_id);
    });
    connect(image_detail_page_, &ImageDetailPage::EditRequested,
            this, [this, image_repo](int64_t image_id) {
        EditImageDialog dlg(image_repo.get(), image_id, this);
        if (dlg.exec() == QDialog::Accepted)
            image_detail_page_->LoadImage(image_id);
    });

    // ── Processing → task record ─────────────────────────────

    connect(process_page_, &ProcessingPage::ProcessingCompleted,
            this, [this, task_repo](int64_t video_id, int64_t image_id,
                 QString backend, double avg_fps, int total_frames,
                 QString output_dir, QString status) {
        OnProcessingCompleted(video_id, image_id, backend, avg_fps,
                              total_frames, output_dir, status);
    });

    // ── View task result from processing page ──────────────────

    connect(process_page_, &ProcessingPage::ViewTaskResultRequested,
            this, [this]() {
        if (last_completed_task_id_ > 0) {
            task_detail_page_->LoadTask(last_completed_task_id_);
            ShowDetailPage(task_detail_page_);
        }
    });

    // ── View CSV from task history ────────────────────────────

    connect(history_page_, &TaskHistoryPage::ViewCsvRequested,
            this, [this](int64_t task_id) {
        OnViewCsv(task_id);
    });
    connect(task_detail_page_, &TaskDetailPage::ViewCsvRequested,
            this, [this](int64_t task_id) {
        OnViewCsv(task_id);
    });
    connect(task_detail_page_, &TaskDetailPage::ViewCleanCsvRequested,
            this, [this](int64_t task_id) {
        OnViewCleanCsv(task_id);
    });

    // ── Edit task from history / detail page ──────────────────

    auto task_repo_ptr = std::make_shared<db::TaskRepo>(database_->Get());
    auto video_repo_ptr = std::make_shared<db::VideoRepo>(database_->Get());
    auto image_repo_ptr = std::make_shared<db::ImageRepo>(database_->Get());

    connect(history_page_, &TaskHistoryPage::EditRequested,
            this, [this, task_repo_ptr, video_repo_ptr, image_repo_ptr](int64_t task_id) {
        EditTaskDialog dlg(task_repo_ptr.get(), video_repo_ptr.get(),
                           image_repo_ptr.get(), task_id, this);
        if (dlg.exec() == QDialog::Accepted)
            history_page_->Refresh();
    });

    connect(task_detail_page_, &TaskDetailPage::EditRequested,
            this, [this, task_repo_ptr, video_repo_ptr, image_repo_ptr](int64_t task_id) {
        EditTaskDialog dlg(task_repo_ptr.get(), video_repo_ptr.get(),
                           image_repo_ptr.get(), task_id, this);
        if (dlg.exec() == QDialog::Accepted)
            task_detail_page_->LoadTask(task_id);
    });

    // ── Processing state for status bar ───────────────────────

    connect(process_page_, &ProcessingPage::ProcessingActive,
            this, [this](bool active) {
        if (active) {
            status_label_->setText(QStringLiteral("处理中..."));
            status_indicator_->setStyleSheet(
                QString("background: %1; border-radius: 6px;")
                    .arg(ThemeManager::Instance().Success()));
        } else {
            status_label_->setText(QStringLiteral("就绪"));
            status_indicator_->setStyleSheet(
                QString("background: %1; border-radius: 6px;")
                    .arg(ThemeManager::Instance().TextDim()));
        }
    });
}

void MainWindow::ShowDetailPage(QWidget* page) {
    previous_page_index_ = content_stack_->currentIndex();
    content_stack_->setCurrentWidget(page);
    if (auto* btn = nav_buttons_->checkedButton())
        nav_buttons_->setExclusive(false);
    if (auto* btn = nav_buttons_->checkedButton())
        btn->setChecked(false);
    nav_buttons_->setExclusive(true);
}

void MainWindow::OnBackFromDetail() {
    // Release resources on the detail page being left so files can be deleted.
    int detailIndex = content_stack_->currentIndex();
    if (detailIndex == 7)
        task_detail_page_->ReleaseResources();
    else if (detailIndex == 5)
        video_detail_page_->ReleaseResources();
    content_stack_->setCurrentIndex(previous_page_index_);
}

void MainWindow::OnPageChanged(int index) {
    int prev = content_stack_->currentIndex();
    if (prev == 1)  // Leaving processing page
        process_page_->ReleaseMediaResources();
    content_stack_->setCurrentIndex(index);
}

void MainWindow::SetupStatusBar() {
    status_label_ = new QLabel(QStringLiteral("就绪"), this);
    frame_label_  = new QLabel(this);
    fps_label_    = new QLabel(this);

    statusBar()->addWidget(status_label_);
    statusBar()->addPermanentWidget(frame_label_);
    statusBar()->addPermanentWidget(fps_label_);

    auto& tm = ThemeManager::Instance();
    statusBar()->setStyleSheet(
        QString("QStatusBar { background: %1; border-top: 1px solid %2; }"
                "QStatusBar::item { border: none; }")
            .arg(tm.DeepBg(), tm.Border()));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (process_page_->IsProcessing()) {
        auto result = QMessageBox::warning(this,
            QStringLiteral("退出确认"),
            QStringLiteral("处理任务正在进行中，关闭窗口将中止任务。\n\n确定退出？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        process_page_->OnStop();
    }
    event->accept();
}

void MainWindow::OnProcessingCompleted(int64_t video_id, int64_t image_id,
                                       QString backend, double avg_fps,
                                       int total_frames, QString output_dir,
                                       QString status) {
    if (video_id <= 0 || image_id <= 0) return;

    auto task_repo = std::make_shared<db::TaskRepo>(database_->Get());

    db::TaskRecord task;
    task.video_id = video_id;
    task.image_id = image_id;
    task.backend = backend.toStdString();
    task.status = status.toStdString();
    task.total_frames = total_frames;
    task.avg_fps = avg_fps;
    task.output_dir = output_dir.toStdString();

    // Build task name: "<video_name> - <backend>"
    auto video_repo = std::make_shared<db::VideoRepo>(database_->Get());
    auto image_repo = std::make_shared<db::ImageRepo>(database_->Get());
    QString video_name;
    QString image_name;
    if (auto v = video_repo->FindById(video_id))
        video_name = QString::fromStdString(v->name);
    if (auto img = image_repo->FindById(image_id))
        image_name = QString::fromStdString(img->name);
    task.name = (video_name + QStringLiteral(" - ") + image_name).toStdString();

    if (!output_dir.isEmpty()) {
        std::string video_path = process_page_->GetWorkerOutputVideoPath();
        if (!video_path.empty())
            task.output_video_path = video_path;

        QString csv_name;
        if (backend == QStringLiteral("openface")) {
            QFileInfo dir_info(output_dir);
            QString dir_basename = dir_info.fileName();
            if (dir_basename.length() > 16)
                csv_name = dir_basename.left(dir_basename.length() - 16) + QStringLiteral(".csv");
        } else if (backend == QStringLiteral("mediapipe"))
            csv_name = QStringLiteral("mediapipe_analysis.csv");
        else if (backend == QStringLiteral("l2cs"))
            csv_name = QStringLiteral("l2cs_analysis.csv");
        else if (backend == QStringLiteral("mefarg_au"))
            csv_name = QStringLiteral("mefarg_au_analysis.csv");
        if (!csv_name.isEmpty())
            task.csv_path = (output_dir + QStringLiteral("/") + csv_name).toStdString();
    }

    // Output types per backend
    if (backend == QStringLiteral("openface"))
        task.output_types = R"(["openface_native","aligned_faces","landmarks_68","head_pose","gaze","au"])";
    else if (backend == QStringLiteral("mediapipe"))
        task.output_types = R"(["landmarks_468","head_pose"])";
    else if (backend == QStringLiteral("l2cs"))
        task.output_types = R"(["gaze_yaw_pitch","gaze_3d_vector"])";
    else if (backend == QStringLiteral("mefarg_au"))
        task.output_types = R"(["au_41"])";
    else
        task.output_types = R"(["face_detection","tracking","recognition"])";

    task.params_yaml = "{}";  // placeholder — full YAML serialization TBD

    try {
        last_completed_task_id_ = task_repo->Insert(task);
        history_page_->Refresh();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, QStringLiteral("保存任务记录失败"),
            QStringLiteral("无法写入任务历史:\n%1").arg(QString::fromStdString(e.what())));
    }
}

void MainWindow::OnViewCsv(int64_t task_id) {
    auto task_repo = std::make_shared<db::TaskRepo>(database_->Get());
    auto task = task_repo->FindById(task_id);
    if (!task || task->csv_path.empty()) {
        QMessageBox::information(this, QStringLiteral("CSV不可用"),
            QStringLiteral("该任务没有CSV输出文件。"));
        return;
    }

    QString csv_path = QString::fromStdString(task->csv_path);
    QFile file(csv_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"),
            QStringLiteral("无法打开CSV文件:\n%1").arg(csv_path));
        return;
    }

    QTextStream in(&file);
    QStringList allLines;
    int lineCount = 0;
    while (!in.atEnd() && lineCount < 1000) {
        allLines << in.readLine();
        lineCount++;
    }

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("CSV数据查看"));
    dlg->resize(900, 500);
    dlg->setMinimumSize(600, 400);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags(dlg->windowFlags() | Qt::WindowMaximizeButtonHint);
    auto& tm = ThemeManager::Instance();
    dlg->setStyleSheet(
        QString("QDialog { background: %1; }").arg(tm.SurfaceBg()));

    auto* layout = new QVBoxLayout(dlg);

    auto* info = new QLabel(QStringLiteral("文件: %1  (%2 行)")
        .arg(csv_path).arg(lineCount), dlg);
    info->setStyleSheet(
        QString("QLabel { color: %1; padding: 4px; }").arg(tm.TextMuted()));
    layout->addWidget(info);

    auto* table = new QTableView(dlg);
    auto* model = new QStandardItemModel(table);

    if (!allLines.isEmpty()) {
        // Header
        QStringList headers = allLines[0].split(QStringLiteral(","));
        model->setColumnCount(headers.size());
        for (int c = 0; c < headers.size(); ++c)
            model->setHeaderData(c, Qt::Horizontal, headers[c].trimmed());

        // Data rows
        for (int r = 1; r < allLines.size(); ++r) {
            QStringList cols = allLines[r].split(QStringLiteral(","));
            for (int c = 0; c < cols.size() && c < headers.size(); ++c) {
                auto* item = new QStandardItem(cols[c].trimmed());
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                model->setItem(r - 1, c, item);
            }
        }
    }

    table->setModel(model);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(true);
    table->setStyleSheet(QString(
        "QTableView { background: %1; color: %2; border: 1px solid %3;"
        "  gridline-color: %3; font-size: 11px;"
        "  selection-background-color: %4; selection-color: %5; }"
        "QHeaderView::section { background: %6; color: %7;"
        "  border: none; border-bottom: 2px solid %3; padding: 4px 8px; }")
        .arg(tm.DeepBg(), tm.TextPrimary(), tm.Border(),
             tm.Primary(), tm.White(), tm.SurfaceBg(), tm.TextMuted()));
    layout->addWidget(table, 1);

    auto* btn_row = new QHBoxLayout();
    btn_row->addStretch();

    auto* export_btn = new QPushButton(QStringLiteral("导出CSV"), dlg);
    export_btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background: %4; color: %5; }")
        .arg(tm.SuccessBg(), tm.SuccessText(), tm.Success(), tm.SuccessHover(), tm.White()));
    QString csvPath = csv_path;
    connect(export_btn, &QPushButton::clicked, this, [this, csvPath]() {
        QString savePath = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出CSV文件"),
            QFileInfo(csvPath).fileName(),
            QStringLiteral("CSV 文件 (*.csv)"));
        if (savePath.isEmpty()) return;
        if (!QFile::copy(csvPath, savePath)) {
            QMessageBox::warning(this, QStringLiteral("导出失败"),
                QStringLiteral("无法导出CSV文件到:\n%1").arg(savePath));
        }
    });
    btn_row->addWidget(export_btn);

    btn_row->addSpacing(8);

    auto* field_ref_btn = new QPushButton(QStringLiteral("字段解释"), dlg);
    field_ref_btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background: %4; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Primary(), tm.AccentBlueHover()));
    QString backend = QString::fromStdString(task->backend);
    connect(field_ref_btn, &QPushButton::clicked, this, [this, backend]() {
        auto* ref_dlg = new CsvReferenceDialog(nullptr);
        ref_dlg->setAttribute(Qt::WA_DeleteOnClose);
        ref_dlg->SelectBackend(backend);
        ref_dlg->show();
    });
    btn_row->addWidget(field_ref_btn);

    btn_row->addSpacing(8);

    auto* close_btn = new QPushButton(QStringLiteral("关闭"), dlg);
    close_btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 8px 24px; }"
        "QPushButton:hover { background: %4; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(close_btn, &QPushButton::clicked, dlg, &QDialog::close);
    btn_row->addWidget(close_btn);

    btn_row->addStretch();
    layout->addLayout(btn_row);

    dlg->show();
}

void MainWindow::OnViewCleanCsv(int64_t task_id) {
    auto task_repo = std::make_shared<db::TaskRepo>(database_->Get());
    auto task = task_repo->FindById(task_id);
    if (!task || task->csv_path.empty()) {
        QMessageBox::information(this, QStringLiteral("CSV不可用"),
            QStringLiteral("该任务没有CSV输出文件。"));
        return;
    }

    QFileInfo fi(QString::fromStdString(task->csv_path));
    QString clean_csv_path = fi.dir().filePath(fi.completeBaseName() + QStringLiteral("_clean.csv"));

    QFile file(clean_csv_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"),
            QStringLiteral("未找到清洗后CSV文件，请先执行数据清洗。\n\n%1").arg(clean_csv_path));
        return;
    }

    QTextStream in(&file);
    QStringList allLines;
    int lineCount = 0;
    while (!in.atEnd() && lineCount < 1000) {
        allLines << in.readLine();
        lineCount++;
    }

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("清洗后CSV数据查看"));
    dlg->resize(900, 500);
    dlg->setMinimumSize(600, 400);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags(dlg->windowFlags() | Qt::WindowMaximizeButtonHint);
    auto& tm = ThemeManager::Instance();
    dlg->setStyleSheet(
        QString("QDialog { background: %1; }").arg(tm.SurfaceBg()));

    auto* layout = new QVBoxLayout(dlg);

    auto* info = new QLabel(QStringLiteral("清洗后文件: %1  (%2 行)")
        .arg(clean_csv_path).arg(lineCount), dlg);
    info->setStyleSheet(
        QString("QLabel { color: %1; padding: 4px; }").arg(tm.TextMuted()));
    layout->addWidget(info);

    auto* table = new QTableView(dlg);
    auto* model = new QStandardItemModel(table);

    if (!allLines.isEmpty()) {
        QStringList headers = allLines[0].split(QStringLiteral(","));
        model->setColumnCount(headers.size());
        for (int c = 0; c < headers.size(); ++c)
            model->setHeaderData(c, Qt::Horizontal, headers[c].trimmed());

        for (int r = 1; r < allLines.size(); ++r) {
            QStringList cols = allLines[r].split(QStringLiteral(","));
            for (int c = 0; c < cols.size() && c < headers.size(); ++c) {
                auto* item = new QStandardItem(cols[c].trimmed());
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                model->setItem(r - 1, c, item);
            }
        }
    }

    table->setModel(model);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(true);
    table->setStyleSheet(QString(
        "QTableView { background: %1; color: %2; border: 1px solid %3;"
        "  gridline-color: %3; font-size: 11px;"
        "  selection-background-color: %4; selection-color: %5; }"
        "QHeaderView::section { background: %6; color: %7;"
        "  border: none; border-bottom: 2px solid %3; padding: 4px 8px; }")
        .arg(tm.DeepBg(), tm.TextPrimary(), tm.Border(),
             tm.Primary(), tm.White(), tm.SurfaceBg(), tm.TextMuted()));
    layout->addWidget(table, 1);

    auto* btn_row = new QHBoxLayout();
    btn_row->addStretch();

    auto* export_btn = new QPushButton(QStringLiteral("导出CSV"), dlg);
    export_btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background: %4; color: %5; }")
        .arg(tm.SuccessBg(), tm.SuccessText(), tm.Success(), tm.SuccessHover(), tm.White()));
    QString csvPath = clean_csv_path;
    connect(export_btn, &QPushButton::clicked, this, [this, csvPath]() {
        QString savePath = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出清洗后CSV文件"),
            QFileInfo(csvPath).fileName(),
            QStringLiteral("CSV 文件 (*.csv)"));
        if (savePath.isEmpty()) return;
        if (!QFile::copy(csvPath, savePath)) {
            QMessageBox::warning(this, QStringLiteral("导出失败"),
                QStringLiteral("无法导出CSV文件到:\n%1").arg(savePath));
        }
    });
    btn_row->addWidget(export_btn);

    btn_row->addSpacing(8);

    auto* field_ref_btn = new QPushButton(QStringLiteral("字段解释"), dlg);
    field_ref_btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background: %4; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Primary(), tm.AccentBlueHover()));
    QString backend = QString::fromStdString(task->backend);
    connect(field_ref_btn, &QPushButton::clicked, this, [this, backend]() {
        auto* ref_dlg = new CsvReferenceDialog(nullptr);
        ref_dlg->setAttribute(Qt::WA_DeleteOnClose);
        ref_dlg->SelectBackend(backend);
        ref_dlg->show();
    });
    btn_row->addWidget(field_ref_btn);

    btn_row->addSpacing(8);

    auto* close_btn = new QPushButton(QStringLiteral("关闭"), dlg);
    close_btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 8px 24px; }"
        "QPushButton:hover { background: %4; }")
        .arg(tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(close_btn, &QPushButton::clicked, dlg, &QDialog::close);
    btn_row->addWidget(close_btn);

    btn_row->addStretch();
    layout->addLayout(btn_row);

    dlg->show();
}

}  // namespace ui
}  // namespace omniface
