#include "kv/cli.hpp"
#include "kv/engine.hpp"
#include "kv/regcmd.hpp"

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace {

// 会话默认缓存容量
constexpr std::size_t kDefaultCapacity = 64;

// 空输入时的命令列表提示
void print_commands()
{
    std::cerr << "commands:\n";
    for (const auto& k : kv::commands()) {
        std::cerr << "    " << k.usage << '\n';
    }
    std::cerr << '\n';
}

}  // namespace

int main(int argc, char* argv[])
{
    kv::Engine engine{kDefaultCapacity};

    // argv 单条命令
    if (argc > 1) {
        return kv::exec(engine, std::vector<std::string>(argv + 1, argv + argc),
                        std::cout, std::cerr);
    }

    // 进入 stdin 先进行命令提示
    print_commands();

    std::string line;
    bool has_error = false;
    while (std::getline(std::cin, line)) {
        if (kv::exec(engine, kv::tokenize(line), std::cout, std::cerr) != 0) {
            has_error = true;
        }
    }

    return has_error ? 2 : 0;
}
