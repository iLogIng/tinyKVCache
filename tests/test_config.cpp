#include "kv/config.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace kv;

namespace {

// 把字符串数组转成 argv 形式
std::vector<char*> argv_of(std::vector<std::string>& args)
{
    std::vector<char*> argv;
    for (std::string& s : args) {
        argv.push_back(s.data());
    }
    return argv;
}

}  // namespace

TEST_CASE("server 参数默认值")
{
    std::vector<std::string> args = {"prog"};
    std::vector<char*> argv = argv_of(args);
    ServerConfig cfg;
    std::string err;
    REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 0);
    REQUIRE(cfg.bind == "127.0.0.1");
    REQUIRE(cfg.port == kDefaultPort);
    REQUIRE(cfg.capacity == 64);
    REQUIRE(cfg.aof_path == "kv.aof");
    REQUIRE(cfg.fsync == Fsync::Always);
}

TEST_CASE("server 参数逐项解析")
{
    std::vector<std::string> args = {
        "prog", "--bind", "0.0.0.0", "--port", "9000", "--capacity", "128",
        "--aof", "/tmp/x.aof", "--fsync", "os"};
    std::vector<char*> argv = argv_of(args);
    ServerConfig cfg;
    std::string err;
    REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 0);
    REQUIRE(cfg.bind == "0.0.0.0");
    REQUIRE(cfg.port == 9000);
    REQUIRE(cfg.capacity == 128);
    REQUIRE(cfg.aof_path == "/tmp/x.aof");
    REQUIRE(cfg.fsync == Fsync::Os);
}

TEST_CASE("server 参数非法输入")
{
    ServerConfig cfg;
    std::string err;
    {
        std::vector<std::string> args = {"prog", "--port"};
        std::vector<char*> argv = argv_of(args);
        REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 1);
        REQUIRE_FALSE(err.empty());
    }
    {
        std::vector<std::string> args = {"prog", "--fsync", "never"};
        std::vector<char*> argv = argv_of(args);
        REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 1);
    }
    {
        std::vector<std::string> args = {"prog", "--capacity", "0"};
        std::vector<char*> argv = argv_of(args);
        REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 1);
    }
    {
        std::vector<std::string> args = {"prog", "--nope"};
        std::vector<char*> argv = argv_of(args);
        REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 1);
    }
}

TEST_CASE("server --help 打印用法")
{
    std::vector<std::string> args = {"prog", "--help"};
    std::vector<char*> argv = argv_of(args);
    ServerConfig cfg;
    std::string err;
    REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 2);
}

TEST_CASE("server group 模式与刷盘间隔")
{
    {
        std::vector<std::string> args = {"prog", "--fsync", "group", "--fsync-interval", "5"};
        std::vector<char*> argv = argv_of(args);
        ServerConfig cfg;
        std::string err;
        REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 0);
        REQUIRE(cfg.fsync == Fsync::Group);
        REQUIRE(cfg.fsync_interval_ms == 5);
    }
    {
        std::vector<std::string> args = {"prog", "--fsync", "bad"};
        std::vector<char*> argv = argv_of(args);
        ServerConfig cfg;
        std::string err;
        REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 1);
    }
    {
        std::vector<std::string> args = {"prog", "--fsync-interval", "-1"};
        std::vector<char*> argv = argv_of(args);
        ServerConfig cfg;
        std::string err;
        REQUIRE(parse_server_args(static_cast<int>(argv.size()), argv.data(), cfg, err) == 1);
    }
}

TEST_CASE("client 参数解析与命令位置")
{
    ClientConfig cfg;
    std::string err;
    int first = 0;
    {
        std::vector<std::string> args = {"prog", "--host", "10.0.0.1", "--port", "9000",
                                         "get", "k"};
        std::vector<char*> argv = argv_of(args);
        REQUIRE(parse_client_args(static_cast<int>(argv.size()), argv.data(), cfg, first, err) == 0);
        REQUIRE(cfg.host == "10.0.0.1");
        REQUIRE(cfg.port == 9000);
        REQUIRE(args[first] == "get");
    }
    {
        // 无选项: 首个参数即命令
        std::vector<std::string> args = {"prog", "lst"};
        std::vector<char*> argv = argv_of(args);
        ClientConfig c2;
        int pos = 0;
        REQUIRE(parse_client_args(static_cast<int>(argv.size()), argv.data(), c2, pos, err) == 0);
        REQUIRE(pos == 1);
    }
}
