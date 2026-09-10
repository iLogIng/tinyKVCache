#include "kv/net.hpp"

#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace kv;

TEST_CASE("net 编解码 round-trip")
{
    {
        const std::string body = encode_request("put", {"k", "v"});
        REQUIRE(!body.empty());
        std::string cmd;
        std::vector<std::string> args;
        REQUIRE(decode_request(body, cmd, args));
        REQUIRE(cmd == "put");
        REQUIRE(args == std::vector<std::string>({"k", "v"}));
    }
    {
        // 无参数命令
        const std::string body = encode_request("lst", {});
        std::string cmd;
        std::vector<std::string> args;
        REQUIRE(decode_request(body, cmd, args));
        REQUIRE(cmd == "lst");
        REQUIRE(args.empty());
    }
    {
        // 空参数可表达
        const std::string body = encode_request("put", {"k", ""});
        std::string cmd;
        std::vector<std::string> args;
        REQUIRE(decode_request(body, cmd, args));
        REQUIRE(args.size() == 2);
        REQUIRE(args[1].empty());
    }
    {
        // 含任意字节(二进制)参数
        const std::string val("a\nb\0c", 5);
        const std::string body = encode_request("put", {"k", val});
        std::string cmd;
        std::vector<std::string> args;
        REQUIRE(decode_request(body, cmd, args));
        REQUIRE(args[1] == val);
    }
}

TEST_CASE("net 编解码边界与非法输入")
{
    // 命令名过长(>255) -> 编码失败
    REQUIRE(encode_request(std::string(256, 'a'), {}).empty());
    // 命令名为空 -> 编码失败
    REQUIRE(encode_request("", {}).empty());
    // 参数个数 >255 -> 编码失败
    REQUIRE(encode_request("put", std::vector<std::string>(256, "x")).empty());

    std::string cmd;
    std::vector<std::string> args;
    // body 过短
    REQUIRE_FALSE(decode_request("", cmd, args));
    REQUIRE_FALSE(decode_request("x", cmd, args));
    // cmd_len = 0
    REQUIRE_FALSE(decode_request(std::string("\x00\x00", 2), cmd, args));
    // 声明了参数但数据不足
    const std::string truncated = std::string("\x01\x01z\x10\x00\x00\x00", 7);
    REQUIRE_FALSE(decode_request(truncated, cmd, args));

    // 尾部多余字节被忽略
    std::string body = encode_request("get", {"k"});
    body.append("extra");
    REQUIRE(decode_request(body, cmd, args));
    REQUIRE(cmd == "get");
    REQUIRE(args == std::vector<std::string>({"k"}));
}

TEST_CASE("net 帧读写与版本校验")
{
    int fds[2];
    REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // 正常帧往返(含空 body)
    REQUIRE(write_frame(fds[0], "hello"));
    std::string body;
    REQUIRE(read_frame(fds[1], body));
    REQUIRE(body == "hello");

    REQUIRE(write_frame(fds[0], ""));
    REQUIRE(read_frame(fds[1], body));
    REQUIRE(body.empty());

    // 版本不匹配
    REQUIRE(send_all(fds[0], std::string("\x02\x00\x00\x00\x00", 5)));
    REQUIRE_FALSE(read_frame(fds[1], body));

    ::close(fds[0]);
    ::close(fds[1]);
}
