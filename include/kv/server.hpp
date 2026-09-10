#ifndef KV_SERVER_HPP
#define KV_SERVER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "kv/engine.hpp"
#include "kv/journal.hpp"

namespace kv {

// 网络服务: 单线程多连接, 监听并执行请求
class Server {
public:
    Server(std::uint16_t port, std::size_t capacity, std::string aof_path,
           Fsync fsync);

    // 建立监听并进入事件循环
    bool run();

private:
    struct Conn {
        int fd;
        std::string in;   // 接收缓冲
        std::string out;  // 发送缓冲
        bool gone = false;
    };
    struct Reply {
        std::string body;   // 响应帧内容
        bool close = false; // 是否断开
    };

    static void set_nonblock(int fd);
    bool setup();
    Reply run_request(const std::vector<std::string>& tokens);
    void on_readable(Conn& c);
    void on_writable(Conn& c);

    std::uint16_t port_;
    std::size_t capacity_;
    std::string aof_path_;
    Fsync fsync_;
    int lfd_ = -1;
    Engine engine_;
    Journal journal_;
    std::vector<Conn> conns_;
};

}  // namespace kv

#endif  // KV_SERVER_HPP
