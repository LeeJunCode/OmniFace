#include <QApplication>
#include <QDir>
#include <QIcon>

#include <opencv2/core/mat.hpp>

#include "omniface/ui/main_window.h"

int main(int argc, char* argv[]) {
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");

    QApplication app(argc, argv);
    app.setApplicationName("OmniFace");
    app.setApplicationVersion("1.0.0");

    qRegisterMetaType<cv::Mat>("cv::Mat");

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
