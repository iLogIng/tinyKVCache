#include "kv/cli.hpp"
#include "kv/engine.hpp"
#include "kv/net.hpp"
#include "kv/regcmd.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

using kv::Engine;
using kv::kDefaultPort;

void set_nonblock(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

struct Conn
{
    int fd;
    std::string in;    // 未成行的读缓冲
    std::string out;   // 待发送缓冲
    bool gone = false;
};

struct Reply
{
    std::string body;  // 已含空行帧尾
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

// 有可读事件: 读入缓冲并按 \n 切出请求逐条执行
void on_readable(Engine& engine, Conn& c)
{
    char buf[4096];
    for (;;) {
        const ssize_t n = ::recv(c.fd, buf, sizeof(buf), 0);
        if (n > 0) {
            c.in.append(buf, static_cast<std::size_t>(n));
            for (;;) {
                const std::size_t pos = c.in.find('\n');
                if (pos == std::string::npos) {
                    break;
                }
                std::string line = c.in.substr(0, pos);
                c.in.erase(0, pos + 1);
                if (line.size() > kv::kMaxLine) {
                    c.gone = true;
                    return;
                }
                const Reply r = run_request(engine, kv::tokenize(line));
                if (!r.body.empty()) {
                    c.out += r.body;
                }
                if (r.close) {
                    c.gone = true;
                    return;
                }
            }
            if (c.in.size() > kv::kMaxLine) {
                c.gone = true;
            }
        }
        else if (n == 0) {
            c.gone = true;  // 对端关闭
            return;
        }
        else if (errno == EINTR) {
            continue;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        else {
            c.gone = true;
            return;
        }
    }
}

// 有可写事件: 尽量发完 out 缓冲
void on_writable(Conn& c)
{
    if (c.out.empty()) {
        return;
    }
    const ssize_t n = ::send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);
    if (n > 0) {
        c.out.erase(0, static_cast<std::size_t>(n));
    }
    else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;
    }
    else if (n < 0 && errno == EINTR) {
        return;
    }
    else {
        c.gone = true;
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
    set_nonblock(lfd);

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
    std::vector<Conn> conns;

    for (;;) {
        fd_set rfds, wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(lfd, &rfds);
        int maxfd = lfd;
        for (const Conn& c : conns) {
            FD_SET(c.fd, &rfds);
            if (!c.out.empty()) {
                FD_SET(c.fd, &wfds);
            }
            if (c.fd > maxfd) {
                maxfd = c.fd;
            }
        }

        if (::select(maxfd + 1, &rfds, &wfds, nullptr, nullptr) < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "error: select()\n";
            break;
        }

        // 新连接
        if (FD_ISSET(lfd, &rfds)) {
            for (;;) {
                const int cfd = ::accept(lfd, nullptr, nullptr);
                if (cfd < 0) {
                    break;  // EAGAIN: 本轮接受完
                }
                set_nonblock(cfd);
                conns.push_back(Conn{cfd, {}, {}, false});
            }
        }

        // 读写事件
        for (Conn& c : conns) {
            if (c.gone) {
                continue;
            }
            if (FD_ISSET(c.fd, &rfds)) {
                on_readable(engine, c);
            }
            if (!c.gone && !c.out.empty() && FD_ISSET(c.fd, &wfds)) {
                on_writable(c);
            }
        }

        // 清理关闭的连接
        conns.erase(std::remove_if(conns.begin(), conns.end(),
                                   [](const Conn& c) {
                                       if (c.gone) {
                                           ::close(c.fd);
                                       }
                                       return c.gone;
                                   }),
                    conns.end());
    }

    ::close(lfd);
    return 0;
}
