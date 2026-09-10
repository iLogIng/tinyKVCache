#include "kv/server.hpp"

#include "kv/net.hpp"
#include "kv/regcmd.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace kv {

Server::Server(std::uint16_t port, std::size_t capacity, std::string aof_path,
               Fsync fsync)
    : port_(port)
    , capacity_(capacity)
    , aof_path_(std::move(aof_path))
    , fsync_(fsync)
    , engine_(capacity)
{
}

void Server::set_nonblock(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool Server::setup()
{
    lfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (lfd_ < 0) {
        std::cerr << "error: socket()\n";
        return false;
    }
    int reuse = 1;
    ::setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    set_nonblock(lfd_);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port_);
    if (::bind(lfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "error: bind(" << port_ << ")\n";
        ::close(lfd_);
        lfd_ = -1;
        return false;
    }
    if (::listen(lfd_, 16) < 0) {
        std::cerr << "error: listen()\n";
        ::close(lfd_);
        lfd_ = -1;
        return false;
    }
    return true;
}

// 一条请求 -> 响应; exit/quit 触发 close
Server::Reply Server::run_request(const std::vector<std::string>& tokens)
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
    exec(engine_, tokens, out);
    reply.body = out.str();
    return reply;
}

// 可读: 读入缓冲并按帧切分请求逐条执行
void Server::on_readable(Conn& c)
{
    char buf[4096];
    for (;;) {
        const ssize_t n = ::recv(c.fd, buf, sizeof(buf), 0);
        if (n > 0) {
            c.in.append(buf, static_cast<std::size_t>(n));
            for (;;) {
                if (c.in.size() < 5) {
                    break;  // 帧头不完整
                }
                if (static_cast<std::uint8_t>(c.in[0]) != kProtocolVersion) {
                    c.gone = true;
                    return;
                }
                std::uint32_t body_len = 0;
                body_len = static_cast<std::uint8_t>(c.in[1])
                    | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c.in[2])) << 8)
                    | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c.in[3])) << 16)
                    | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c.in[4])) << 24);
                if (body_len > kMaxFrame) {
                    c.gone = true;
                    return;
                }
                if (c.in.size() < 5 + body_len) {
                    break;  // body 不完整
                }
                const std::string body = c.in.substr(5, body_len);
                c.in.erase(0, 5 + body_len);

                std::string cmd;
                std::vector<std::string> args;
                if (!decode_request(body, cmd, args)) {
                    c.gone = true;
                    return;
                }
                std::vector<std::string> tokens;
                tokens.reserve(args.size() + 1);
                tokens.push_back(std::move(cmd));
                tokens.insert(tokens.end(), args.begin(), args.end());

                const Reply r = run_request(tokens);
                if (!r.close) {
                    c.out += encode_frame(r.body);
                }
                else {
                    c.gone = true;
                    return;
                }
            }
        }
        else if (n == 0) {
            c.gone = true;
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

// 可写: 尽量发完 out 缓冲
void Server::on_writable(Conn& c)
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
        std::cerr << "server: send error on fd " << c.fd << ": "
                  << std::strerror(errno) << '\n';
        c.gone = true;
    }
}

bool Server::run()
{
    if (!setup()) {
        return false;
    }

    std::cout << "kv-server 127.0.0.1:" << port_
              << " capacity=" << capacity_ << '\n';

    // 持久化: 回放历史写序列后挂接日志
    if (journal_.open(aof_path_, fsync_)) {
        engine_.replay(journal_);
        engine_.attach(journal_);
    }
    else {
        std::cerr << "engine: persistence disabled (" << aof_path_ << ")\n";
    }

    for (;;) {
        // 先算最大 fd, 防止 FD_SET 越界
        int maxfd = lfd_;
        for (const Conn& c : conns_) {
            if (c.fd > maxfd) {
                maxfd = c.fd;
            }
        }
        if (maxfd + 1 > FD_SETSIZE) {
            std::cerr << "server: fd overflow, drop fd " << maxfd << '\n';
            conns_.erase(
                std::remove_if(conns_.begin(), conns_.end(),
                    [maxfd](Conn& c) {
                        if (c.fd == maxfd) {
                            ::close(c.fd);
                            return true;
                        }
                        return false;
                    }),
                conns_.end());
            continue;
        }

        fd_set rfds, wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(lfd_, &rfds);
        for (const Conn& c : conns_) {
            FD_SET(c.fd, &rfds);
            if (!c.out.empty()) {
                FD_SET(c.fd, &wfds);
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
        if (FD_ISSET(lfd_, &rfds)) {
            for (;;) {
                const int cfd = ::accept(lfd_, nullptr, nullptr);
                if (cfd < 0) {
                    break;  // EAGAIN: 本轮接受完
                }
                if (conns_.size() + 1 >= static_cast<std::size_t>(FD_SETSIZE) - 16) {
                    std::cerr << "server: connection limit reached, reject fd "
                              << cfd << '\n';
                    ::close(cfd);
                    continue;
                }
                set_nonblock(cfd);
                conns_.push_back(Conn{cfd, {}, {}, false});
            }
        }

        // 读写事件
        for (Conn& c : conns_) {
            if (c.gone) {
                continue;
            }
            if (FD_ISSET(c.fd, &rfds)) {
                on_readable(c);
            }
            if (!c.gone && !c.out.empty() && FD_ISSET(c.fd, &wfds)) {
                on_writable(c);
            }
        }

        // 清理关闭的连接
        conns_.erase(
            std::remove_if(conns_.begin(), conns_.end(),
                [](const Conn& c) {
                    if (c.gone) {
                        ::close(c.fd);
                    }
                    return c.gone;
                }),
            conns_.end());
    }

    ::close(lfd_);
    lfd_ = -1;
    return true;
}

}  // namespace kv
