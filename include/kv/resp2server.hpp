#ifndef KV_RESP2SERVER_HPP
#define KV_RESP2SERVER_HPP

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "kv/config.hpp"
#include "kv/engine.hpp"
#include "kv/journal.hpp"

namespace kv {

// 执行一条 RESP2 命令, 应答写入 out
void resp2_dispatch(Engine& engine, const std::vector<std::string>& args,
                    std::string& out);

// RESP2 网络服务
// 单线程多连接, 监听并执行请求
class Resp2Server {
public:
    explicit Resp2Server(ServerConfig config);

    // 建立监听并进入事件循环
    bool run();

private:
    // 连接
    struct Conn {
        int fd;
        std::string in;             // 读缓冲
        std::string out;            // 写缓冲
        std::size_t in_off = 0;     // 读缓冲已消费前缀
        bool pending_sync = false;  // 有写响应等待落盘
        bool peer_closed = false;   // 对端已关闭写端
        bool gone = false;
    };

    static void set_nonblock(int fd);
    bool setup();
    void update_events(Conn& c);    // 按写缓冲增删 EPOLLOUT
    void sync_and_release();        // 刷盘后放行待发送响应
    void on_readable(Conn& c);
    void on_writable(Conn& c);

    static constexpr int kMaxEvents = 64;   // epoll_wait 单次事件上限

    ServerConfig config_;                   // 配置
    int lfd_ = -1;                          // 监听文件描述符
    int epfd_ = -1;                         // epoll 实例
    Engine engine_;                         // kv 引擎
    Journal journal_;                       // 回放日志
    std::unordered_map<int, Conn> conns_;   // 连接
};

}  // namespace kv

#endif  // KV_RESP2SERVER_HPP
