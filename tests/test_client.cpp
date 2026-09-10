#include "kv/client.hpp"
#include "kv/net.hpp"

#include <arpa/inet.h>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace kv;

namespace {

// 进程内假服务端: 每收到一帧回 "r<n>"; max_replies>=0 时回够即断开
class FakeServer {
public:
    explicit FakeServer(int max_replies = -1)
        : max_replies_(max_replies)
    {
        lfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        int reuse = 1;
        ::setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        ::bind(lfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        socklen_t len = sizeof(addr);
        ::getsockname(lfd_, reinterpret_cast<sockaddr*>(&addr), &len);
        port_ = ntohs(addr.sin_port);
        ::listen(lfd_, 4);
        thread_ = std::thread([this] { serve(); });
    }

    ~FakeServer()
    {
        ::shutdown(lfd_, SHUT_RDWR);
        ::close(lfd_);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    std::uint16_t port() const { return port_; }

private:
    void serve()
    {
        const int cfd = ::accept(lfd_, nullptr, nullptr);
        if (cfd < 0) {
            return;
        }
        std::string body;
        int n = 0;
        while (read_frame(cfd, body)) {
            if (!write_frame(cfd, "r" + std::to_string(n))) {
                break;
            }
            ++n;
            if (max_replies_ >= 0 && n >= max_replies_) {
                break;
            }
        }
        ::close(cfd);
    }

    int max_replies_;
    int lfd_ = -1;
    std::uint16_t port_ = 0;
    std::thread thread_;
};

}  // namespace

TEST_CASE("client 命令工厂")
{
    const Request put = cmd_put("k", "v");
    REQUIRE(put.cmd == "put");
    REQUIRE(put.args == std::vector<std::string>({"k", "v"}));

    const Request get = cmd_get("k");
    REQUIRE(get.cmd == "get");
    REQUIRE(get.args == std::vector<std::string>({"k"}));

    const Request clr = cmd_clr();
    REQUIRE(clr.cmd == "clr");
    REQUIRE(clr.args.empty());
}

TEST_CASE("client 单请求与批量请求")
{
    FakeServer server;
    Client c;
    REQUIRE(c.connect("127.0.0.1", server.port()));
    REQUIRE(c.is_open());

    std::string payload;
    REQUIRE(c.request(cmd_get("k"), payload));
    REQUIRE(payload == "r0");

    const std::vector<Request> batch = {cmd_get("a"), cmd_get("b"), cmd_lst()};
    std::vector<std::string> out;
    REQUIRE(c.request_batch(batch, out));
    REQUIRE(out == std::vector<std::string>({"r1", "r2", "r3"}));

    c.close();
    REQUIRE_FALSE(c.is_open());
}

TEST_CASE("client 未连接与断连返回失败")
{
    Client c;
    std::string payload;
    REQUIRE_FALSE(c.send(cmd_get("k")));
    REQUIRE_FALSE(c.recv(payload));

    // 服务端只回一次即断开
    FakeServer server(1);
    REQUIRE(c.connect("127.0.0.1", server.port()));
    REQUIRE(c.request(cmd_get("k"), payload));
    REQUIRE_FALSE(c.request(cmd_get("k"), payload));
}

TEST_CASE("client 连接被拒返回失败")
{
    // 先占端口再关闭, 确保该端口无监听
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t len = sizeof(addr);
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    const std::uint16_t port = ntohs(addr.sin_port);
    ::close(fd);

    Client c;
    REQUIRE_FALSE(c.connect("127.0.0.1", port));
}
