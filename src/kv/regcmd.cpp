#include "kv/regcmd.hpp"
#include "kv/cli.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace kv {

void cmd_put(Engine& e, const Command& c)
{
    e.put(c.args[0], c.args[1]);
}

void cmd_get(Engine& e, const Command& c)
{
    const std::string& key = c.args[0];
    if (auto value = e.get(key)) {
        std::cout << key << "->" << *value << '\n';
    }
    else {
        std::cout << '\'' << key << "' not found\n";
    }
}

void cmd_del(Engine& e, const Command& c)
{
    e.erase(c.args[0]);
}

void cmd_clr(Engine& e, const Command&)
{
    e.clear();
}

void cmd_lst(Engine& e, const Command&)
{
    auto items = e.items();
    std::sort(items.begin(), items.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [key, value] : items) {
        std::cout << key << ',' << value << '\n';
    }
}

void cmd_exit(Engine&, const Command&)
{
    std::exit(0);
}

void cmd_help(Engine&, const Command&);

/**
 * 命令表
 * 语法, 行为
*/
const CommandSpec kSpecs[] = {
    { "put",  2, 2, "put <key> <value>", cmd_put },
    { "get",  1, 1, "get <key>",         cmd_get },
    { "del",  1, 1, "del <key>",         cmd_del },
    { "clr",  0, 0, "clr",               cmd_clr },
    { "lst",  0, 0, "lst",               cmd_lst },
    { "exit", 0, 0, "exit",              cmd_exit },
    { "quit", 0, 0, "quit",              cmd_exit },
    { "help", 0, 0, "help",              cmd_help },
};

void cmd_help(Engine&, const Command&)
{
    for (const auto& k : kSpecs) {
        std::cout << "    " << k.usage << '\n';
    }
}

/**
 * 查表校验并执行。空 tokens 跳过；
 * 解析/参数错误打印 stderr 并返回 2。
*/
int exec(Engine& engine, const std::vector<std::string>& tokens)
{
    if (tokens.empty()) {
        return 0;
    }

    Command cmd;
    switch (parse(tokens, cmd)) {
        case ParseError::Ok:
            break;
        case ParseError::UnknownCommand:
            std::cerr << "error: unknown command '" << tokens[0] << "'\n";
            return 2;
        case ParseError::WrongArgCount:
            std::cerr << "error: wrong argument count for '" << tokens[0] << "'\n"
                      << "  usage: " << usage_of(tokens[0]) << '\n';
            return 2;
    }

    for (const auto& k : kSpecs) {
        if (k.name == cmd.name) {
            k.handler(engine, cmd);
            return 0;
        }
    }
    return 2;
}

}  // namespace kv
