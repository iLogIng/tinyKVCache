#include "kv/config.hpp"
#include "kv/server.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    kv::ServerConfig config;
    std::string err;
    const int rc = kv::parse_server_args(argc, argv, config, err);
    // --help
    if (rc == 2) {
        return 0;
    }
    // error
    if (rc == 1) {
        std::cerr << "error: " << err << '\n';
        kv::print_server_usage();
        return 1;
    }

    kv::Server server(std::move(config));
    return server.run() ? 0 : 1;
}
