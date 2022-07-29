#include "webserver/webserver.h"

int main() {
    /*    // 守护进程 后台运行
        daemon(1, 1);*/

    const std::string config_file_name = "config.yaml";

    WebServer server(config_file_name);
    server.Start();

    return 0;
}
