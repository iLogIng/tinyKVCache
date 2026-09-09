#include "kv/regcmd.hpp"
#include "kv/cli.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <ostream>
#include <sstream>
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

// 命令表
// 语法, 行为
const CommandSpec kSpecs[] = {
    { "put",  2, 2, "put <key> <value>", cmd_put },
    { "get",  1, 1, "get <key>",         cmd_get },
    { "del",  1, 1, "del <key>",         cmd_del },
    { "clr",  0, 0, "clr",               cmd_clr },
    { "lst",  0, 0, "lst",               cmd_lst },
    { "help", 0, 0, "help",              cmd_help },
};

// 命令表视图
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

// 查表校验并执行。空 tokens 跳过。
// 成功: 结果写 out 返回 nullopt; 失败: 返回错误文本, out 不变。
std::optional<std::string> exec(Engine& engine,
                                const std::vector<std::string>& tokens,
                                std::ostream& out)
{
    if (tokens.empty()) {
        return std::nullopt;
    }

    Command cmd;
    switch (parse(tokens, cmd)) {
        case ParseError::Ok:
            break;
        case ParseError::UnknownCommand: {
            std::ostringstream msg;
            msg << "error: unknown command '" << tokens[0] << "'\n";
            return msg.str();
        }
        case ParseError::WrongArgCount: {
            std::ostringstream msg;
            msg << "error: wrong argument count for '" << tokens[0] << "'\n"
                << "  usage: " << usage_of(tokens[0]) << '\n';
            return msg.str();
        }
    }

    for (const auto& k : commands()) {
        if (k.name == cmd.name) {
            k.handler(engine, cmd, out);
            return std::nullopt;
        }
    }
    std::ostringstream msg;
    msg << "error: unknown command '" << tokens[0] << "'\n";
    return msg.str();
}

}  // namespace kv
