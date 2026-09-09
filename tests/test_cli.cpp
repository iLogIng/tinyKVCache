#include "kv/cli.hpp"
#include "kv/engine.hpp"
#include "kv/regcmd.hpp"

#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace kv;

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

    // 空 tokens 跳过: 返回 nullopt, 无输出
    out.str("");
    REQUIRE(!exec(engine, {}, out).has_value());
    REQUIRE(out.str().empty());

    // 坏命令: 错误在返回值, out 不变, 引擎状态不变
    const std::size_t before = engine.size();
    out.str("");
    auto e1 = exec(engine, {"badcmd", "x"}, out);
    REQUIRE(e1.has_value());
    REQUIRE(e1->find("unknown command 'badcmd'") != std::string::npos);
    REQUIRE(out.str().empty());

    out.str("");
    auto e2 = exec(engine, {"put", "only"}, out);
    REQUIRE(e2.has_value());
    REQUIRE(e2->find("usage: put <key> <value>") != std::string::npos);
    REQUIRE(engine.size() == before);

    // 正常会话
    out.str("");
    REQUIRE(!exec(engine, {"put", "a", "1"}, out).has_value());
    out.str("");
    REQUIRE(!exec(engine, {"put", "b", "2"}, out).has_value());

    out.str("");
    REQUIRE(!exec(engine, {"get", "a"}, out).has_value());
    REQUIRE(out.str() == "a->1\n");

    out.str("");
    REQUIRE(!exec(engine, {"get", "missing"}, out).has_value());
    REQUIRE(out.str() == "'missing' not found\n");

    out.str("");
    REQUIRE(!exec(engine, {"lst"}, out).has_value());
    REQUIRE(out.str() == "a,1\nb,2\n");

    out.str("");
    REQUIRE(!exec(engine, {"help"}, out).has_value());
    REQUIRE(out.str().find("help") != std::string::npos);

    out.str("");
    REQUIRE(!exec(engine, {"del", "a"}, out).has_value());
    REQUIRE(engine.get("a") == std::nullopt);
    out.str("");
    REQUIRE(!exec(engine, {"del", "missing"}, out).has_value());

    out.str("");
    REQUIRE(!exec(engine, {"clr"}, out).has_value());
    REQUIRE(engine.size() == 0);
}
