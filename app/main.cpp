#include <filesystem>
#include <iostream>
#include <string>

#include <QApplication>

#include "omniface/pipeline/config.h"
#include "omniface/ui/main_window.h"

int main(int argc, char* argv[]) {
    using namespace omniface;

    qRegisterMetaType<omniface::ThresholdConfig>("ThresholdConfig");

    // Default config path: next to executable
    std::string config_path = "config.yaml";

    // Parse CLI args
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    // Load config
    PipelineConfig config;
    if (std::filesystem::exists(config_path)) {
        try {
            config = LoadPipelineConfig(config_path);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to load config '" << config_path
                      << "': " << e.what() << "\nUsing defaults." << std::endl;
        }
    } else {
        std::cerr << "Warning: Config file '" << config_path
                  << "' not found. Using defaults." << std::endl;
    }

    QApplication app(argc, argv);
    app.setApplicationName("OmniFace");
    app.setApplicationVersion("1.0.0");

    omniface::ui::MainWindow window(config);
    window.show();

    return app.exec();
}
