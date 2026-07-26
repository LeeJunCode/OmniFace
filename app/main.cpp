#include <filesystem>
#include <iostream>
#include <string>

#include <QApplication>

#include "omniface/pipeline/config.h"
#include "omniface/ui/main_window.h"

int main(int argc, char* argv[]) {
    using namespace omniface;

    // 注册自定义类型，使ThresholdConfig可通过跨线程信号/槽传递
    qRegisterMetaType<omniface::ThresholdConfig>("ThresholdConfig");

    std::string config_path = "config.yaml";

    // 解析--config命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    // 尝试加载配置文件，失败则使用默认配置
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
