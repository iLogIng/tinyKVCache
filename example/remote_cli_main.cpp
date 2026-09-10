#include "kv/cli.hpp"
#include "kv/net.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int main(int argc, char* argv[])
{
    const unsigned short port = argc > 1
        ? static_cast<unsigned short>(std::strtoul(argv[1], nullptr, 10))
        : kv::kDefaultPort;

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "error: socket()\n";
        return 1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "error: connect(" << port << ")\n";
        ::close(fd);
        return 1;
    }

    // 逐行解析用户输入, 编码为帧发送并打印响应
    std::string line;
    while (std::getline(std::cin, line)) {
        std::vector<std::string> tokens = kv::tokenize(line);
        if (tokens.empty()) {
            continue;
        }
        if (tokens[0] == "exit" || tokens[0] == "quit") {
            break;
        }
        const std::string cmd = tokens[0];
        const std::vector<std::string> args(tokens.begin() + 1, tokens.end());
        const std::string body = kv::encode_request(cmd, args);
        if (body.empty()) {
            std::cerr << "error: invalid command\n";
            continue;
        }
        if (!kv::write_frame(fd, body)) {
            break;
        }
        std::string payload;
        if (!kv::read_frame(fd, payload)) {
            break;
        }
        std::cout << payload;
        std::cout.flush();
    }

    ::close(fd);
    return 0;
}
