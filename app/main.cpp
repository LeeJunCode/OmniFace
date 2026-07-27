#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <QApplication>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

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
        if (arg == "--version" || arg == "-v") {
            std::cout << "1.0.0" << std::endl;
            return 0;
        }
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    // 规范化并验证配置文件路径
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(fs::path(config_path), ec);
    if (ec) {
        std::cerr << "Warning: Cannot resolve config path '" << config_path
                  << "': " << ec.message() << "\nUsing defaults." << std::endl;
    } else {
        config_path = canonical.string();
        auto ext = canonical.extension().string();
        if (ext != ".yaml" && ext != ".yml") {
            std::cerr << "Warning: Config file '" << config_path
                      << "' has unrecognized extension. Expected .yaml or .yml" << std::endl;
        }
    }

    // 尝试加载配置文件，失败则使用默认配置
    PipelineConfig config;
    if (fs::exists(config_path)) {
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

    if (config.output_base_dir.empty()) {
        config.output_base_dir =
            (std::filesystem::path(app.applicationDirPath().toStdString())
             / "data" / "output").string();
    }

    namespace fs = std::filesystem;
    fs::path log_dir = fs::path(app.applicationDirPath().toStdString())
                       / "data" / "log";
    fs::create_directories(log_dir, ec);

    auto now = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    localtime_s(&now_tm, &now_t);
    std::ostringstream log_name;
    log_name << "omniface_"
             << std::put_time(&now_tm, "%Y%m%d_%H%M%S") << ".log";

    fs::path log_file = log_dir / log_name.str();
    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            log_file.string(), true);
        auto logger = std::make_shared<spdlog::logger>("omniface", file_sink);
        logger->set_level(spdlog::level::from_str(config.log.level));
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(logger);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to setup file logging: " << e.what() << std::endl;
    }

    // 清理旧日志：仅保留最近10个日志文件
    {
        std::vector<fs::path> log_files;
        for (const auto& entry : fs::directory_iterator(log_dir, ec)) {
            if (entry.is_regular_file() &&
                entry.path().filename().string().starts_with("omniface_") &&
                entry.path().extension() == ".log") {
                log_files.push_back(entry.path());
            }
        }
        if (log_files.size() > 10) {
            std::sort(log_files.begin(), log_files.end());
            for (size_t i = 0; i < log_files.size() - 10; ++i) {
                fs::remove(log_files[i], ec);
            }
        }
    }

    omniface::ui::MainWindow window(config, config_path);
    window.show();

    return app.exec();
}
