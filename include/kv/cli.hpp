#ifndef KV_CLI_HPP
#define KV_CLI_HPP

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace kv {

class Engine;

enum class ParseError
{
    Ok = 0,
    UnknownCommand,
    WrongArgCount,
};

// 一条已解析的命令
struct Command {
    std::string_view name;          // 命令名称
    std::vector<std::string> args;  // 参数
};

// 命令的声明式描述
struct CommandSpec {
    std::string_view name;                 // 命令名称
    int min_args;                          // 最小参数
    int max_args;                          // 最大参数
    std::string_view usage;                // 命令说明
    void (*handler)(Engine&, const Command&, std::ostream&);  // 命令行为(结果写 out)
};

// 命令表视图: 提供 range-for 能力(begin/end), 免除空名哨兵
struct CommandView {
    const CommandSpec *cmds;
    std::size_t size;
    const CommandSpec* begin() const { return cmds; }
    const CommandSpec* end() const { return cmds + size; }
};

// 全部已注册命令(定义于 regcmd.cpp, 命令表定义处)
CommandView commands();

// 按空白与引号切分一行输入，供交互式输入使用
std::vector<std::string> tokenize(const std::string& line);

// 查表并校验参数个数，产出结构化命令
ParseError parse(const std::vector<std::string>& tokens, Command& out);

// 取命令的 usage 说明，未知命令返回空串
std::string_view usage_of(std::string_view name);

const char* to_string(ParseError err);

}  // namespace kv

#endif  // KV_CLI_HPP
