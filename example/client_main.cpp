#include "kv/client.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// 用法: KVCache-Client <port> <command> [args...]
int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "<port> <command> [args...]\n";
        return 1;
    }
    const unsigned short port = static_cast<unsigned short>(
        std::strtoul(argv[1], nullptr, 10));

    kv::Request request;
    request.cmd = argv[2];
    for (int i = 3; i < argc; ++i) {
        request.args.emplace_back(argv[i]);
    }

    kv::Client client;
    if (!client.connect("127.0.0.1", port)) {
        std::cerr << "error: connect(" << port << ")\n";
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
