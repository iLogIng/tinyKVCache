#include "kv/cli.hpp"
#include "kv/engine.hpp"
#include "kv/net.hpp"
#include "kv/regcmd.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
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

// 设置不阻塞
void set_nonblock(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 连接
struct Conn
{
    int fd;
    std::string in;    // 未成行的读缓冲
    std::string out;   // 待发送缓冲
    bool gone = false; // 可发送？
};

// 响应
struct Reply
{
    std::string body;   // 已含空行帧尾
    bool close = false; // 断开
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
    std::ostringstream out;
    kv::exec(engine, tokens, out);
    // 响应体: 内容行以 \n 收尾, 再添加 \n 作空行帧尾;
    // 空体(静默成功/出错) -> 单个空行
    std::string body = out.str();
    if (!body.empty() && body.back() != '\n') {
        body += '\n';
    }
    reply.body = std::move(body) + "\n";
    return reply;
}

// 可读事件: 读入缓冲并按 \n 切分请求逐条执行
void on_readable(Engine& engine, Conn& c)
{
    char buf[4096];
    for (;;) {
        // 接收到的字节数量
        const ssize_t n = ::recv(c.fd, buf, sizeof(buf), 0);
        if (n > 0) {
            // 将接收的 buf 附加到接收缓冲
            c.in.append(buf, static_cast<std::size_t>(n));
            for (;;) {
                // 按 \n 切分
                const std::size_t pos = c.in.find('\n');
                if (pos == std::string::npos) {
                    break;
                }
                // 取出一行命令
                std::string line = c.in.substr(0, pos);
                c.in.erase(0, pos + 1);
                // 检查是否在限制大小内
                if (line.size() > kv::kMaxLine) {
                    c.gone = true;
                    return;
                }
                // 执行请求
                const Reply r = run_request(engine, kv::tokenize(line));
                if (!r.body.empty()) {
                    c.out += r.body;
                }
                // 关闭连接
                if (r.close) {
                    c.gone = true;
                    return;
                }
            }
            // 大于最大可返回行
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
            std::cerr << "server: recv error on fd " << c.fd << ": "
                      << std::strerror(errno) << '\n';
            c.gone = true;
            return;
        }
    }
}

// 可写事件: 尽量发完 out 缓冲
void on_writable(Conn& c)
{
    // 响应缓冲为空
    if (c.out.empty()) {
        return;
    }
    // 发送的缓冲长度
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
        std::cerr << "server: send error on fd " << c.fd << ": "
                  << std::strerror(errno) << '\n';
        c.gone = true;
    }
}

}  // namespace

int main(int argc, char* argv[])
{
    if (argc == 1 || argc > 3) {
        std::cerr << "<port> <capacity>\n";
        return 1;
    }
    const unsigned short port = argc > 1
        ? static_cast<unsigned short>(std::strtoul(argv[1], nullptr, 10))
        : kDefaultPort;
    const std::size_t capacity = argc > 2
        ? static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10))
        : 64;

    // 建立连接
    const int lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        std::cerr << "error: socket()\n";
        return 1;
    }
    int reuse = 1;
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    set_nonblock(lfd);

    // 连接地址
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

    Engine engine{capacity};
    // 多个连接
    std::vector<Conn> conns;

    for (;;) {
        // 先计算最大 fd, 防止 FD_SET 越界
        int maxfd = lfd;
        for (const Conn& c : conns) {
            if (c.fd > maxfd) {
                maxfd = c.fd;
            }
        }
        if (maxfd + 1 > FD_SETSIZE) {
            std::cerr << "server: fd overflow, drop fd " << maxfd << '\n';
            conns.erase(
                std::remove_if(conns.begin(), conns.end(),
                    [maxfd](Conn& c) {
                        if (c.fd == maxfd) {
                            ::close(c.fd);
                            return true;
                        }
                        return false;
                    }),
                conns.end());
            continue;
        }

        // 文件描述符集
        fd_set rfds, wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(lfd, &rfds);
        for (const Conn& c : conns) {
            FD_SET(c.fd, &rfds);
            if (!c.out.empty()) {
                FD_SET(c.fd, &wfds);
            }
        }

        // 核心 网络select
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
                // 等待连接到达
                const int cfd = ::accept(lfd, nullptr, nullptr);
                if (cfd < 0) {
                    break;  // EAGAIN: 本轮接受完
                }
                // 连接数接近 FD_SETSIZE 时拒绝新连接
                if (conns.size() + 1 >= static_cast<std::size_t>(FD_SETSIZE) - 16) {
                    std::cerr << "server: connection limit reached, reject fd "
                              << cfd << '\n';
                    ::close(cfd);
                    continue;
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
            // 读
            if (FD_ISSET(c.fd, &rfds)) {
                on_readable(engine, c);
            }
            // 写
            if (!c.gone && !c.out.empty() && FD_ISSET(c.fd, &wfds)) {
                on_writable(c);
            }
        }

        // 清理关闭的连接
        conns.erase(
            std::remove_if(
                conns.begin(), conns.end(),
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
