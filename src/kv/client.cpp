#include "kv/net.hpp"

#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

using kv::kDefaultPort;

}  // namespace

int main(int argc, char* argv[])
{
    const unsigned short port = argc > 1
        ? static_cast<unsigned short>(std::strtoul(argv[1], nullptr, 10))
        : kDefaultPort;

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

    // 逐行转发 stdin 请求, 打印响应(去空行帧尾)
    std::string line, resp;
    while (std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") {
            break;
        }
        if (!kv::send_all(fd, line + "\n")) {
            break;
        }
        if (kv::recv_frame(fd, resp)) {
            std::cout << resp;
            std::cout.flush();
        }
        else {
            break;
        }
    }

    ::close(fd);
    return 0;
}
