#include <string>

#include "omniface/pipeline/config.h"
#include "omniface/pipeline/pipeline.h"
#include "omniface/server/web_server.h"

using omniface::pipeline::ConfigLoader;
using omniface::pipeline::Pipeline;

int main(int argc, char* argv[]) {
    std::string config_path = "config.yaml";
    bool serve_mode = false;
    int serve_port = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--serve") {
            serve_mode = true;
        } else if (arg == "--port" && i + 1 < argc) {
            serve_port = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            return 0;
        }
    }

    auto cfg = ConfigLoader::Load(config_path);
    ConfigLoader::ApplyOverrides(cfg, argc, argv);

    if (serve_mode) {
        int port = serve_port > 0 ? serve_port : cfg.server_port;
        omniface::server::WebServer server(cfg);
        server.Run(cfg.server_host, port);
        return 0;
    }

    Pipeline pipeline(cfg);

    if (!pipeline.Initialize()) {
        return 1;
    }

    try {
        pipeline.Run();
    } catch (const std::exception& e) {
        pipeline.Shutdown();
        return 1;
    }
    pipeline.Shutdown();

    return 0;
}
