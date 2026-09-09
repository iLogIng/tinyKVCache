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
    Engine e(8);
    std::ostringstream out, err;
    const auto run = [&](const std::vector<std::string>& tokens) {
        out.str("");
        err.str("");
        return exec(e, tokens, out, err);
    };

    // 空行跳过
    REQUIRE(run({}) == 0);
    REQUIRE(out.str().empty());

    // 坏命令: 返回 2, 错误进 err, 不改变状态
    const std::size_t before = e.size();
    REQUIRE(run({"badcmd", "x"}) == 2);
    REQUIRE(err.str().find("unknown command 'badcmd'") != std::string::npos);
    REQUIRE(run({"put", "only"}) == 2);
    REQUIRE(err.str().find("usage: put <key> <value>") != std::string::npos);
    REQUIRE(e.size() == before);

    // 正常会话
    REQUIRE(run({"put", "a", "1"}) == 0);
    REQUIRE(run({"put", "b", "2"}) == 0);
    REQUIRE(run({"get", "a"}) == 0);
    REQUIRE(out.str() == "a->1\n");

    REQUIRE(run({"get", "missing"}) == 0);
    REQUIRE(out.str() == "'missing' not found\n");

    REQUIRE(run({"lst"}) == 0);
    REQUIRE(out.str() == "a,1\nb,2\n");

    REQUIRE(run({"help"}) == 0);
    REQUIRE(out.str().find("help") != std::string::npos);

    REQUIRE(run({"del", "a"}) == 0);
    REQUIRE(e.get("a") == std::nullopt);
    REQUIRE(run({"del", "missing"}) == 0);

    REQUIRE(run({"clr"}) == 0);
    REQUIRE(e.size() == 0);
}
