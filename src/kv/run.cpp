#include "kv/cli.hpp"
#include "kv/engine.hpp"
#include "kv/regcmd.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

using kv::kSpecs;

// 空输入时的命令列表提示
void print_commands()
{
    std::cerr << "commands:\n";
    for (size_t i = 0; kSpecs[i].name.size() > 0; ++i) {
        std::cerr << "    " << kSpecs[i].usage << '\n';
    }
    std::cerr << '\n';
}

}  // namespace

int main(int argc, char* argv[])
{
    kv::Engine engine;

    // argv 单条命令
    if (argc > 1) {
        return kv::exec(engine, std::vector<std::string>(argv + 1, argv + argc));
    }

    // 进入 stdin 先进行命令提示
    print_commands();

    std::string line;
    bool has_error = false;
    while (std::getline(std::cin, line)) {
        if (kv::exec(engine, kv::tokenize(line)) != 0) {
            has_error = true;
        }
    }

    return has_error ? 2 : 0;
}
