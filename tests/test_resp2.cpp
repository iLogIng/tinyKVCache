#include "kv/engine.hpp"
#include "kv/resp2.hpp"
#include "kv/resp2server.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace kv;

namespace {

// 构造一条 RESP2 命令
std::string request(const std::vector<std::string>& args)
{
    std::string out = "*" + std::to_string(args.size()) + "\r\n";
    for (const std::string& a : args) {
        out += "$" + std::to_string(a.size()) + "\r\n" + a + "\r\n";
    }
    return out;
}

}  // namespace

TEST_CASE("resp2 解析: 完整命令")
{
    std::string buf = request({"SET", "k", "v"});
    std::size_t off = 0;
    std::vector<std::string> args;
    REQUIRE(resp2_parse(buf, off, args) == Resp2Status::Ok);
    REQUIRE(args == std::vector<std::string>({"SET", "k", "v"}));
    REQUIRE(off == buf.size());
}

TEST_CASE("resp2 解析: 半包与 pipeline")
{
    std::string buf = request({"PING"}) + request({"GET", "k"});
    std::size_t off = 0;
    std::vector<std::string> args;

    // 第一条命令只给一部分 -> Incomplete
    const std::string one = request({"PING"});
    std::string_view part(one.data(), one.size() - 1);
    std::size_t poff = 0;
    REQUIRE(resp2_parse(part, poff, args) == Resp2Status::Incomplete);
    REQUIRE(poff == 0);

    // 完整后连续解析两条
    REQUIRE(resp2_parse(buf, off, args) == Resp2Status::Ok);
    REQUIRE(args == std::vector<std::string>({"PING"}));
    REQUIRE(resp2_parse(buf, off, args) == Resp2Status::Ok);
    REQUIRE(args == std::vector<std::string>({"GET", "k"}));
    REQUIRE(off == buf.size());
}

TEST_CASE("resp2 解析: 非法输入")
{
    std::size_t off = 0;
    std::vector<std::string> args;

    std::string not_array = "+OK\r\n";
    off = 0;
    REQUIRE(resp2_parse(not_array, off, args) == Resp2Status::Invalid);

    std::string bad_count = "*x\r\n";
    off = 0;
    REQUIRE(resp2_parse(bad_count, off, args) == Resp2Status::Invalid);

    std::string bad_len = "*1\r\n$2\r\nabxx";
    off = 0;
    REQUIRE(resp2_parse(bad_len, off, args) == Resp2Status::Invalid);

    std::string missing_crlf = "*1\r\n$4\r\nPINGxx";
    off = 0;
    REQUIRE(resp2_parse(missing_crlf, off, args) == Resp2Status::Invalid);
}

TEST_CASE("resp2 序列化")
{
    REQUIRE(resp2_simple("OK") == "+OK\r\n");
    REQUIRE(resp2_error("ERR x") == "-ERR x\r\n");
    REQUIRE(resp2_integer(1) == ":1\r\n");
    REQUIRE(resp2_integer(0) == ":0\r\n");
    REQUIRE(resp2_bulk("v") == "$1\r\nv\r\n");
    REQUIRE(resp2_bulk("") == "$0\r\n\r\n");
    REQUIRE(resp2_null_bulk() == "$-1\r\n");
}

TEST_CASE("resp2 命令分发")
{
    Engine engine(8);
    std::string out;

    resp2_dispatch(engine, {"PING"}, out);
    REQUIRE(out == "+PONG\r\n");

    resp2_dispatch(engine, {"SET", "k", "v"}, out);
    REQUIRE(out == "+OK\r\n");
    REQUIRE(engine.get("k") == std::optional<std::string>("v"));

    resp2_dispatch(engine, {"GET", "k"}, out);
    REQUIRE(out == "$1\r\nv\r\n");

    resp2_dispatch(engine, {"GET", "missing"}, out);
    REQUIRE(out == "$-1\r\n");

    resp2_dispatch(engine, {"EXISTS", "k"}, out);
    REQUIRE(out == ":1\r\n");
    resp2_dispatch(engine, {"EXISTS", "missing"}, out);
    REQUIRE(out == ":0\r\n");

    resp2_dispatch(engine, {"DEL", "k"}, out);
    REQUIRE(out == ":1\r\n");
    resp2_dispatch(engine, {"DEL", "k"}, out);
    REQUIRE(out == ":0\r\n");

    resp2_dispatch(engine, {"NOPE"}, out);
    REQUIRE(out == "-ERR unknown command 'NOPE'\r\n");

    resp2_dispatch(engine, {"SET", "k"}, out);
    REQUIRE(out == "-ERR wrong number of arguments for 'set'\r\n");
}
