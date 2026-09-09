#include "kv/cli.hpp"
#include "kv/engine.hpp"
#include "kv/net.hpp"
#include "kv/regcmd.hpp"

#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

using kv::Engine;
using kv::kDefaultPort;

struct Reply
{
    std::string body;  // 已含各输出行与空行帧尾
    bool close = false;
};

// 一条请求 -> 响应帧; exit/quit 触发 close
Reply run_request(Engine& engine, const std::vector<std::string>& tokens)
{
    Reply reply;
    if (tokens.empty()) {
        return reply;
    }
    if (tokens[0] == "exit" || tokens[0] == "quit") {
        reply.close = true;
        return reply;
    }
    std::ostringstream out, err;
    kv::exec(engine, tokens, out, err);
    reply.body = out.str() + err.str() + "\n";  // 空行帧尾
    return reply;
}

// 顺序处理一个连接的所有请求(逐行), 直到 EOF 或 exit/quit
void serve_conn(Engine& engine, int fd)
{
    std::string line;
    while (kv::read_line(fd, line)) {
        const Reply reply = run_request(engine, kv::tokenize(line));
        if (!reply.body.empty() && !kv::send_all(fd, reply.body)) {
            break;
        }
        if (reply.close) {
            break;
        }
    }
}

}  // namespace

int main(int argc, char* argv[])
{
    const unsigned short port = argc > 1
        ? static_cast<unsigned short>(std::strtoul(argv[1], nullptr, 10))
        : kDefaultPort;
    const std::size_t capacity = argc > 2
        ? static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10))
        : 64;

    const int lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        std::cerr << "error: socket()\n";
        return 1;
    }
    int reuse = 1;
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (::bind(lfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "error: bind(" << port << ")\n";
        ::close(lfd);
        return 1;
    }
    if (::listen(lfd, 16) < 0) {
        std::cerr << "error: listen()\n";
        ::close(lfd);
        return 1;
    }

    std::cout << "kv-server 127.0.0.1:" << port
              << " capacity=" << capacity << '\n';

    Engine engine(capacity);
    for (;;) {
        const int cfd = ::accept(lfd, nullptr, nullptr);
        if (cfd < 0) {
            std::cerr << "error: accept()\n";
            continue;
        }
        serve_conn(engine, cfd);
        ::close(cfd);
    }
}
