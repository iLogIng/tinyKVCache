#ifndef KV_CLIENT_HPP
#define KV_CLIENT_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace kv {

// 一条请求: 命令名 + 参数(原样, 无转义)
struct Request {
    std::string cmd;
    std::vector<std::string> args;
};

// 命令工厂: 编译期校验参数个数
Request cmd_put(std::string key, std::string value);
Request cmd_get(std::string key);
Request cmd_del(std::string key);
Request cmd_clr();
Request cmd_lst();
Request cmd_help();

// 端到端客户端: 连接服务端, 发送请求并接收响应
class Client {
public:
    Client() = default;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    ~Client();

    bool connect(const std::string& host, unsigned short port);
    void close();
    bool is_open() const;

    bool send(const Request& request);
    bool recv(std::string& payload);

    bool send_batch(const std::vector<Request>& requests);
    bool recv_batch(std::size_t n, std::vector<std::string>& out);

    bool request(const Request& request, std::string& payload);
    bool request_batch(const std::vector<Request>& requests,
                       std::vector<std::string>& out);

private:
    int fd_ = -1;
};

}  // namespace kv

#endif  // KV_CLIENT_HPP
