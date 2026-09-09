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

// 捕获 exec 内部打印到 std::cerr 的错误文本
struct CerrCapture
{
    std::ostringstream sink;
    std::streambuf* old;
    CerrCapture() : old(std::cerr.rdbuf(sink.rdbuf())) {}
    ~CerrCapture() { std::cerr.rdbuf(old); }
    std::string str() const { return sink.str(); }
};

}  // namespace

TEST_CASE("tokenize 边界: 空白/引号/转义/空引号/空行")
{
    REQUIRE(tokenize("put a b") == std::vector<std::string>({"put", "a", "b"}));
    REQUIRE(tokenize("put k \"hello world\"") ==
            std::vector<std::string>({"put", "k", "hello world"}));
    REQUIRE(tokenize("put k 'hello world'") ==
            std::vector<std::string>({"put", "k", "hello world"}));
    REQUIRE(tokenize("put k \"a,b c\"") ==
            std::vector<std::string>({"put", "k", "a,b c"}));
    REQUIRE(tokenize("put k \"say \\\"hi\\\"\"") ==
            std::vector<std::string>({"put", "k", "say \"hi\""}));
    REQUIRE(tokenize("put a\\ b c") ==
            std::vector<std::string>({"put", "a b", "c"}));
    REQUIRE(tokenize("put k \"\"") ==  // 空引号不产生 token
            std::vector<std::string>({"put", "k"}));
    REQUIRE(tokenize("").empty());
    REQUIRE(tokenize("   \t ").empty());
    REQUIRE(tokenize("put\tk\tv") ==
            std::vector<std::string>({"put", "k", "v"}));
}

TEST_CASE("parse 边界: 未知命令/参数个数/大小写/空输入")
{
    Command cmd;

    // 空 tokens 视为 Ok(上层自行跳过)
    REQUIRE(parse({}, cmd) == ParseError::Ok);

    REQUIRE(parse({"nope"}, cmd) == ParseError::UnknownCommand);

    // 大小写敏感
    REQUIRE(parse({"PUT", "k", "v"}, cmd) == ParseError::UnknownCommand);

    // 参数个数低于/高于 min/max
    REQUIRE(parse({"put", "only"}, cmd) == ParseError::WrongArgCount);
    REQUIRE(parse({"put", "a", "b", "c"}, cmd) == ParseError::WrongArgCount);

    REQUIRE(parse({"put", "k", "v"}, cmd) == ParseError::Ok);
    REQUIRE(cmd.name == "put");
    REQUIRE(cmd.args == std::vector<std::string>({"k", "v"}));
}

TEST_CASE("exec 集成边界: 会话内增删查/坏命令不破坏状态")
{
    Engine engine(8);
    std::ostringstream out;

    // 空 tokens 跳过: 返回 0, 无输出
    out.str("");
    REQUIRE(exec(engine, {}, out) == 0);
    REQUIRE(out.str().empty());

    // 坏命令: 错误打印到 cerr, out 不变, 引擎状态不变
    const std::size_t before = engine.size();
    CerrCapture cap;
    out.str("");
    REQUIRE(exec(engine, {"badcmd", "x"}, out) == 2);
    REQUIRE(cap.str().find("unknown command 'badcmd'") != std::string::npos);
    REQUIRE(out.str().empty());

    cap.sink.str("");
    out.str("");
    REQUIRE(exec(engine, {"put", "only"}, out) == 2);
    REQUIRE(cap.str().find("usage: put <key> <value>") != std::string::npos);
    REQUIRE(engine.size() == before);

    // 正常会话
    out.str("");
    REQUIRE(exec(engine, {"put", "a", "1"}, out) == 0);
    out.str("");
    REQUIRE(exec(engine, {"put", "b", "2"}, out) == 0);

    out.str("");
    REQUIRE(exec(engine, {"get", "a"}, out) == 0);
    REQUIRE(out.str() == "a->1\n");

    out.str("");
    REQUIRE(exec(engine, {"get", "missing"}, out) == 0);
    REQUIRE(out.str() == "'missing' not found\n");

    out.str("");
    REQUIRE(exec(engine, {"lst"}, out) == 0);
    REQUIRE(out.str() == "a,1\nb,2\n");

    out.str("");
    REQUIRE(exec(engine, {"help"}, out) == 0);
    REQUIRE(out.str().find("help") != std::string::npos);

    out.str("");
    REQUIRE(exec(engine, {"del", "a"}, out) == 0);
    REQUIRE(engine.get("a") == std::nullopt);
    out.str("");
    REQUIRE(exec(engine, {"del", "missing"}, out) == 0);

    out.str("");
    REQUIRE(exec(engine, {"clr"}, out) == 0);
    REQUIRE(engine.size() == 0);
}
