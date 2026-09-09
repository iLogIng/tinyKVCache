#include "kv/regcmd.hpp"
#include "kv/cli.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace kv {

void cmd_put(Engine& e, const Command& c, std::ostream&)
{
    e.put(c.args[0], c.args[1]);
}

void cmd_get(Engine& e, const Command& c, std::ostream& out)
{
    const std::string& key = c.args[0];
    if (auto value = e.get(key)) {
        out << key << "->" << *value << '\n';
    }
    else {
        out << '\'' << key << "' not found\n";
    }
}

void cmd_del(Engine& e, const Command& c, std::ostream&)
{
    e.erase(c.args[0]);
}

void cmd_clr(Engine& e, const Command&, std::ostream&)
{
    e.clear();
}

void cmd_lst(Engine& e, const Command&, std::ostream& out)
{
    auto items = e.items();
    std::sort(items.begin(), items.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [key, value] : items) {
        out << key << ',' << value << '\n';
    }
}

void cmd_help(Engine&, const Command&, std::ostream& out);

/**
 * 命令表
 * 语法, 行为
 * 数量由 commands() 视图提供, 无需哨兵
*/
const CommandSpec kSpecs[] = {
    { "put",  2, 2, "put <key> <value>", cmd_put },
    { "get",  1, 1, "get <key>",         cmd_get },
    { "del",  1, 1, "del <key>",         cmd_del },
    { "clr",  0, 0, "clr",               cmd_clr },
    { "lst",  0, 0, "lst",               cmd_lst },
    { "help", 0, 0, "help",              cmd_help },
};

// 命令表视图(表界仅在定义处可知)
CommandView commands()
{
    return { kSpecs, std::size(kSpecs) };
}

void cmd_help(Engine&, const Command&, std::ostream& out)
{
    for (const auto& k : commands()) {
        out << "    " << k.usage << '\n';
    }
}

/**
 * 查表校验并执行。空 tokens 跳过；
 * 解析/参数错误打印 err 并返回 2。
*/
int exec(Engine& engine, const std::vector<std::string>& tokens,
         std::ostream& out, std::ostream& err)
{
    if (tokens.empty()) {
        return 0;
    }

    Command cmd;
    switch (parse(tokens, cmd)) {
        case ParseError::Ok:
            break;
        case ParseError::UnknownCommand:
            err << "error: unknown command '" << tokens[0] << "'\n";
            return 2;
        case ParseError::WrongArgCount:
            err << "error: wrong argument count for '" << tokens[0] << "'\n"
                << "  usage: " << usage_of(tokens[0]) << '\n';
            return 2;
    }

    for (const auto& k : commands()) {
        if (k.name == cmd.name) {
            k.handler(engine, cmd, out);
            return 0;
        }
    }
    return 2;
}

}  // namespace kv
