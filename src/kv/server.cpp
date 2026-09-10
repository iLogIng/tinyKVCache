#include "kv/server.hpp"

#include "kv/net.hpp"
#include "kv/regcmd.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <sstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace kv {
Server::Server(ServerConfig config)
    : config_(std::move(config))
    , engine_(config_.capacity)
{ }

// 设置文件非阻塞读写
void Server::set_nonblock(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 服务器初始化
bool Server::setup()
{
    // 创建套接字
    lfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (lfd_ < 0) {
        std::cerr << "error: socket()\n";
        return false;
    }
    int reuse = 1;
    ::setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)); // 地址复用
    set_nonblock(lfd_); // 文件非阻塞读写

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    // 解析绑定地址
    if (::inet_pton(AF_INET, config_.bind.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "error: invalid bind address: " << config_.bind << '\n';
        ::close(lfd_);
        lfd_ = -1;
        return false;
    }
    // 地址绑定
    if (::bind(lfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "error: bind(" << config_.bind << ':' << config_.port << ")\n";
        ::close(lfd_);
        lfd_ = -1;
        return false;
    }
    // 开始监听, 接受队列采用系统上限
    if (::listen(lfd_, SOMAXCONN) < 0) {
        std::cerr << "error: listen()\n";
        ::close(lfd_);
        lfd_ = -1;
        return false;
    }
    // 创建 epoll 实例并注册监听 fd
    epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ < 0) {
        std::cerr << "error: epoll_create1()\n";
        ::close(lfd_);
        lfd_ = -1;
        return false;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = lfd_;
    if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, lfd_, &ev) < 0) {
        std::cerr << "error: epoll_ctl(ADD listen): " << std::strerror(errno) << '\n';
        ::close(epfd_);
        ::close(lfd_);
        epfd_ = -1;
        lfd_ = -1;
        return false;
    }
    return true;
}

// 按发送缓冲增删 EPOLLOUT; 有待落盘写响应时不发
void Server::update_events(Conn& c)
{
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLRDHUP;
    // 输出缓冲非空且无待落盘响应
    if (!c.out.empty() && !c.pending_sync) {
        ev.events |= EPOLLOUT;
    }
    ev.data.fd = c.fd;
    if (::epoll_ctl(epfd_, EPOLL_CTL_MOD, c.fd, &ev) < 0) {
        std::cerr << "server: epoll_ctl(MOD) fd '" << c.fd << "': "
                  << std::strerror(errno) << '\n';
        c.gone = true;
    }
}

// 刷盘后放行待发送响应; 失败则关闭待落盘连接
void Server::sync_and_release()
{
    if (!journal_.sync()) {
        std::cerr << "server: fsync failed, closing pending connections\n";
        for (auto& [fd, c] : conns_) {
            if (c.pending_sync) {
                c.gone = true;
            }
        }
        return;
    }
    for (auto& [fd, c] : conns_) {
        if (c.pending_sync) {
            c.pending_sync = false;
            if (!c.gone) {
                update_events(c);
            }
        }
    }
}

// 请求 -> 响应;
// exit/quit 关闭连接
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
            // 用游标逐帧解析, 避免每帧拷贝与移位
            for (;;) {
                std::string_view body;
                const FrameStatus st = next_frame(c.in, c.in_off, body);
                if (st == FrameStatus::Incomplete) {
                    break;  // 帧不完整, 等待数据
                }
                if (st == FrameStatus::Invalid) {
                    c.gone = true;
                    return;
                }
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

                const std::uint64_t before_w = journal_.write_count();
                const std::uint64_t before_f = journal_.write_fail_count();
                const Reply r = run_request(tokens);
                if (r.close) {
                    c.gone = true;
                    return;
                }
                if (!append_frame(c.out, r.body)) {
                    c.gone = true;
                    return;
                }
                if (journal_.write_fail_count() != before_f) {
                    c.gone = true;  // 记日志失败, 关闭连接
                    return;
                }
                if (journal_.write_count() != before_w) {
                    if (config_.fsync == Fsync::Group) {
                        c.pending_sync = true;  // group 模式: 写响应等待落盘
                    }
                }
                update_events(c);  // 按待落盘状态决定是否放行
            }
            // 缓冲全消费后清空
            if (c.in_off == c.in.size()) {
                c.in.clear();
                c.in_off = 0;
            }
            // 擦除已消费缓冲
            else if (c.in_off > 0) {
                c.in.erase(0, c.in_off);
                c.in_off = 0;
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
            update_events(c);
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

// 可写: 尽量发送 out 缓冲
void Server::on_writable(Conn& c)
{
    if (c.out.empty()) {
        return;
    }
    const ssize_t n = ::send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);
    if (n > 0) {
        c.out.erase(0, static_cast<std::size_t>(n));
        if (c.out.empty()) {
            update_events(c);  // 发送完撤销 EPOLLOUT
        }
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

    std::cout << "kv-server " << config_.bind << ':' << config_.port
              << " capacity=" << config_.capacity << '\n';

    // 持久化: 回放历史写序列后挂接日志
    if (journal_.open(config_.aof_path, config_.fsync)) {
        engine_.replay(journal_);
        engine_.attach(journal_);
    }
    else {
        std::cerr << "engine: persistence disabled (" << config_.aof_path << ")\n";
    }

    const std::chrono::milliseconds interval(config_.fsync_interval_ms);
    epoll_event events[kMaxEvents];
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        const auto wait = journal_.time_until_sync(interval, now);
        const int timeout = wait.count() < 0 ? -1 : static_cast<int>(wait.count());
        const int n = ::epoll_wait(epfd_, events, kMaxEvents, timeout);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "error: epoll_wait()\n";
            break;
        }
        if (n == 0) {
            sync_and_release();  // 到达刷盘间隔
            continue;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;
            const std::uint32_t ev = events[i].events;

            // 监听 fd 上的新连接
            if (fd == lfd_) {
                for (;;) {
                    const int cfd = ::accept(lfd_, nullptr, nullptr);
                    if (cfd < 0) {
                        break;  // EAGAIN: 本轮接受完
                    }
                    set_nonblock(cfd);
                    epoll_event cev{};
                    cev.events = EPOLLIN | EPOLLRDHUP;
                    cev.data.fd = cfd;
                    if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, cfd, &cev) < 0) {
                        std::cerr << "server: epoll_ctl(ADD) fd " << cfd << ": "
                                  << std::strerror(errno) << '\n';
                        ::close(cfd);
                        continue;
                    }
                    conns_.emplace(cfd, Conn{cfd, {}, {}, 0, false, false});
                }
                continue;
            }

            auto it = conns_.find(fd);
            if (it == conns_.end()) {
                continue;
            }
            Conn& c = it->second;
            if (ev & (EPOLLERR | EPOLLHUP)) {
                c.gone = true;
            }
            if (!c.gone && (ev & (EPOLLIN | EPOLLRDHUP))) {
                on_readable(c);
            }
            if (!c.gone && (ev & EPOLLOUT)) {
                on_writable(c);
            }
        }

        // 到达刷盘间隔则提交本批写
        if (journal_.dirty_or_interval_sync(interval)) {
            sync_and_release();
        }

        // 清理关闭的连接
        for (auto it = conns_.begin(); it != conns_.end();) {
            if (it->second.gone) {
                ::epoll_ctl(epfd_, EPOLL_CTL_DEL, it->first, nullptr);
                ::close(it->first);
                it = conns_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    ::close(epfd_);
    ::close(lfd_);
    epfd_ = -1;
    lfd_ = -1;
    return true;
}

}  // namespace kv
