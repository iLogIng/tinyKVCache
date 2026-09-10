#include "kv/server.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 5) {
        std::cerr << "<port> [<capacity>] [<aof>] [<always|os>]\n";
        return 1;
    }
    const std::uint16_t port = static_cast<std::uint16_t>(
        std::strtoul(argv[1], nullptr, 10));
    const std::size_t capacity = argc > 2
        ? static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10))
        : 64;
    const char* aof_path = argc > 3 ? argv[3] : "kv.aof";
    kv::Fsync fsync_policy = kv::Fsync::Always;
    if (argc > 4) {
        const std::string policy = argv[4];
        if (policy == "os") {
            fsync_policy = kv::Fsync::Os;
        }
        else if (policy != "always") {
            std::cerr << "<port> [<capacity>] [<aof>] [<always|os>]\n";
            return 1;
        }
    }

    kv::Server server(port, capacity, aof_path, fsync_policy);
    return server.run() ? 0 : 1;
}
