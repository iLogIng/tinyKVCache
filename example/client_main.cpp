#include "kv/client.hpp"
#include "kv/config.hpp"

#include <iostream>
#include <string>

// 用法: KVCache-Client [--host <ip>] [--port <n>] <command> [args...]
int main(int argc, char* argv[])
{
    kv::ClientConfig config;
    std::string err;
    int first_pos = 0;
    const int rc = kv::parse_client_args(argc, argv, config, first_pos, err);
    if (rc == 2) {
        return 0;
    }
    if (rc == 1) {
        std::cerr << "error: " << err << '\n';
        kv::print_client_usage();
        return 1;
    }
    if (first_pos >= argc) {
        kv::print_client_usage();
        return 1;
    }

    kv::Request request;
    request.cmd = argv[first_pos];
    for (int i = first_pos + 1; i < argc; ++i) {
        request.args.emplace_back(argv[i]);
    }

    kv::Client client;
    if (!client.connect(config.host, config.port)) {
        std::cerr << "error: connect(" << config.host << ':' << config.port << ")\n";
        return 1;
    }
    std::string payload;
    if (!client.request(request, payload)) {
        std::cerr << "error: request failed\n";
        return 1;
    }
    std::cout << payload;
    return 0;
}
