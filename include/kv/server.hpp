#ifndef KV_SERVER_HPP
#define KV_SERVER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "kv/config.hpp"
#include "kv/engine.hpp"
#include "kv/journal.hpp"

namespace kv {

// 网络服务
// 单线程多连接, 监听并执行请求
class Server {
public:
    explicit Server(ServerConfig config);

    // 建立监听并进入事件循环
    bool run();

private:
    // 连接
    struct Conn {
        int fd;
        std::string in;     // 未成行的读缓冲
        std::string out;    // 待发送缓冲
        bool gone = false;  // 可发送？
    };
    // 响应
    struct Reply {
        std::string body;   // 响应体
        bool close = false; // 断开
    };

    static void set_nonblock(int fd);
    bool setup();
    Reply run_request(const std::vector<std::string>& tokens);
    void on_readable(Conn& c);
    void on_writable(Conn& c);

    ServerConfig config_;       // 配置
    int lfd_ = -1;              // 监听文件描述符
    Engine engine_;             // kv引擎
    Journal journal_;           // 回放日志
    std::vector<Conn> conns_;   // 连接
};

}  // namespace kv

#endif  // KV_SERVER_HPP
