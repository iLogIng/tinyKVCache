#include "kv/cli.hpp"
#include "kv/engine.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using kv::Command;
using kv::Engine;
using kv::kSpecs;

void cmd_put(Engine& e, const Command& c)
{
    e.put(c.args[0], c.args[1]);
}

void cmd_get(Engine& e, const Command& c)
{
    const std::string& key = c.args[0];
    if (auto value = e.get(key)) {
        std::cout << key << ',' << *value << '\n';
    }
    else {
        std::cout << key << " not found\n";
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
    exit(0);
}

void cmd_help(Engine&, const Command&)
{
    for (size_t i = 0; !kSpecs[i].name.empty(); ++i) {
        std::cout << "    " << kSpecs[i].usage << "\n";
    }
}

struct Binding {
    std::string_view name;
    void (*handler)(Engine&, const Command&);
};

/* 命令名 -> 执行函数。添加命令只需在 kSpecs 加表项并在此登记。 */
const Binding kBindings[] = {
    { "put", cmd_put },
    { "get", cmd_get },
    { "del", cmd_del },
    { "clr", cmd_clr },
    { "lst", cmd_lst },
    { "exit", cmd_exit},
    { "quit", cmd_exit},
    { "help", cmd_help},
};

// 执行命令
void run_cmd(Engine& engine, const Command& cmd)
{
    for (const auto& b : kBindings) {
        if (b.name == cmd.name) {
            b.handler(engine, cmd);
            return;
        }
    }
}

// 打印命令
void print_commands()
{
    std::cerr << "commands:";
    for (size_t i = 0; kSpecs[i].name.size() > 0; ++i) {
        std::cerr << ' ' << kSpecs[i].name;
    }
    std::cerr << '\n';
}

/* 执行一行 token。出错打印到 stderr 并返回非 0，绝不退出进程。 */
int exec_tokens(Engine& engine, const std::vector<std::string>& tokens)
{
    Command cmd;
    switch (kv::parse(tokens, cmd)) {
        case kv::ParseError::Ok:
            break;
        case kv::ParseError::UnknownCommand:
            std::cerr << "error: unknown command '" << tokens[0] << "'\n";
            return 2;
        case kv::ParseError::WrongArgCount:
            std::cerr << "error: wrong argument count for '" << tokens[0] << "'\n";
            std::cerr << "  usage: " << kv::usage_of(tokens[0]) << '\n';
            return 2;
    }
    run_cmd(engine, cmd);
    return 0;
}

}  // namespace

int main(int argc, char* argv[])
{
    Engine engine;

    /* argv 方式：单条命令 */
    if (argc > 1) {
        const std::vector<std::string> tokens(argv + 1, argv + argc);
        return exec_tokens(engine, tokens);
    }

    /* stdin 方式：逐行执行，共享同一引擎 */
    std::string line;
    bool has_error = false;
    std::size_t n_lines = 0;
    while (std::getline(std::cin, line)) {
        ++n_lines;
        if (exec_tokens(engine, kv::tokenize(line)) != 0) {
            has_error = true;
        }
    }
    if (n_lines == 0) {
        print_commands();
        return 2;
    }
    return has_error ? 2 : 0;
}
