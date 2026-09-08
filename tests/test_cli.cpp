#include "kv/cli.hpp"
#include "kv/engine.hpp"
#include "kv/regcmd.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace kv;

namespace {

// exec 触发 handler 的打印
// 测试期间静音 stdout/stderr
struct CoutSilencer
{
    std::ostringstream sink;
    std::ostringstream esink;
    std::streambuf* old_out;
    std::streambuf* old_err;
    CoutSilencer()
        : old_out(std::cout.rdbuf(sink.rdbuf()))
        , old_err(std::cerr.rdbuf(esink.rdbuf()))
    {
    }
    ~CoutSilencer()
    {
        std::cout.rdbuf(old_out);
        std::cerr.rdbuf(old_err);
    }
};

}  // namespace

TEST_CASE("tokenize 边界: 空白/引号/转义/空 token/空行")
{
    REQUIRE(tokenize("put a b") == std::vector<std::string>({"put", "a", "b"}));
    REQUIRE(tokenize("put k \"hello world\"") ==
            std::vector<std::string>({"put", "k", "hello world"}));
    REQUIRE(tokenize("put k \"a,b c\"") ==
            std::vector<std::string>({"put", "k", "a,b c"}));
    REQUIRE(tokenize("put k \"say \\\"hi\\\"\"") ==
            std::vector<std::string>({"put", "k", "say \"hi\""}));
    REQUIRE(tokenize("put a\\ b c") ==
            std::vector<std::string>({"put", "a b", "c"}));
    REQUIRE(tokenize("put k \"\"") ==
            std::vector<std::string>({"put", "k", ""}));
    REQUIRE(tokenize("").empty());
    REQUIRE(tokenize("   \t ").empty());
    REQUIRE(tokenize("put\tk\tv") ==
            std::vector<std::string>({"put", "k", "v"}));
}

TEST_CASE("parse 边界: 未知命令/参数个数/大小写/空输入")
{
    Command cmd;

    // 边界: 空 tokens 状态视为 Ok
    REQUIRE(parse({}, cmd) == ParseError::Ok);

    REQUIRE(parse({"nope"}, cmd) == ParseError::UnknownCommand);

    // 大小写敏感
    REQUIRE(parse({"PUT", "k", "v"}, cmd) == ParseError::UnknownCommand);

    // 边界: 参数个数低于/高于
    REQUIRE(parse({"put", "only"}, cmd) == ParseError::WrongArgCount);
    REQUIRE(parse({"put", "a", "b", "c"}, cmd) == ParseError::WrongArgCount);

    REQUIRE(parse({"put", "k", "v"}, cmd) == ParseError::Ok);
    REQUIRE(cmd.name == "put");
    REQUIRE(cmd.args == std::vector<std::string>({"k", "v"}));
}

TEST_CASE("exec 集成边界: 会话内增删查/坏命令不破坏状态")
{
    Engine e(8);
    CoutSilencer silence;

    // 空行跳过
    REQUIRE(exec(e, {}) == 0);

    // 边界: 坏命令返回 2 且不改变状态
    const std::size_t before = e.size();
    REQUIRE(exec(e, {"badcmd", "x"}) == 2);
    REQUIRE(exec(e, {"put", "only"}) == 2);
    REQUIRE(e.size() == before);

    REQUIRE(exec(e, {"put", "a", "1"}) == 0);
    REQUIRE(exec(e, {"put", "b", "2"}) == 0);
    REQUIRE(e.get("a") == std::optional<std::string>("1"));

    REQUIRE(exec(e, {"get", "a"}) == 0);
    REQUIRE(exec(e, {"lst"}) == 0);
    REQUIRE(exec(e, {"help"}) == 0);

    REQUIRE(exec(e, {"del", "a"}) == 0);
    REQUIRE(e.get("a") == std::nullopt);
    REQUIRE(exec(e, {"del", "missing"}) == 0);

    REQUIRE(exec(e, {"clr"}) == 0);
    REQUIRE(e.size() == 0);
}
