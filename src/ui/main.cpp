#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QIcon>

#include <opencv2/core/mat.hpp>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "omniface/ui/main_window.h"

namespace {

constexpr int kMaxLogFiles = 10;

void InitLogging(const QString& data_dir) {
    QString log_dir = data_dir + QStringLiteral("/log");
    QDir().mkpath(log_dir);

    // Clean old logs, keep only the most recent kMaxLogFiles
    QDir dir(log_dir);
    QStringList filters;
    filters << QStringLiteral("omniface_*.log");
    auto existing = dir.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);
    while (existing.size() > kMaxLogFiles - 1) {
        QFile::remove(existing.first().absoluteFilePath());
        existing.removeFirst();
    }

    // Create timestamped log file
    QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString log_path = log_dir + QStringLiteral("/omniface_") + ts + QStringLiteral(".log");

    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            log_path.toStdString(), true);
        auto logger = std::make_shared<spdlog::logger>("omniface", file_sink);
        logger->set_level(spdlog::level::info);
        spdlog::set_default_logger(logger);
        spdlog::info("Log: {}", log_path.toStdString());
    } catch (...) {}
}

}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");

    QApplication app(argc, argv);
    app.setApplicationName("OmniFace");
    app.setApplicationVersion("1.0.0");

    qRegisterMetaType<cv::Mat>("cv::Mat");

    InitLogging(QApplication::applicationDirPath() + QStringLiteral("/data"));

    // Ensure SVG icon engine plugin directory is found
    QDir appDir(QApplication::applicationDirPath());
    if (appDir.cd("iconengines")) {
        QApplication::addLibraryPath(appDir.absolutePath());
        appDir.cdUp();
    }

    omniface::ui::MainWindow window;
    window.show();

    return app.exec();
}
