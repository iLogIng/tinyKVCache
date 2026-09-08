#ifndef KV_CLI_HPP
#define KV_CLI_HPP

#include <string>
#include <string_view>
#include <vector>

namespace kv {

class Engine;  // 仅前置声明，供 handler 签名使用

enum class ParseError {
    Ok = 0,
    UnknownCommand,
    WrongArgCount,
};

/* 一条已解析的命令 */
struct Command {
    std::string_view name;          // 命令名称
    std::vector<std::string> args;  // 参数
};

/* 命令的声明式描述：语法 + 行为（handler 定义在 regcmd.cpp） */
struct CommandSpec {
    std::string_view name;                 // 命令名称
    int min_args;                          // 最小参数
    int max_args;                          // 最大参数
    std::string_view usage;                // 命令说明
    void (*handler)(Engine&, const Command&);  // 命令行为
};

/* 命令表，以空 name 结尾 */
extern const CommandSpec kSpecs[];

/* 按空白与引号切分一行输入，供交互式输入使用 */
std::vector<std::string> tokenize(const std::string& line);

/* 查表并校验参数个数，产出结构化命令 */
ParseError parse(const std::vector<std::string>& tokens, Command& out);

/* 取命令的 usage 说明，未知命令返回空串 */
std::string_view usage_of(std::string_view name);

const char* to_string(ParseError err);

}  // namespace kv

#endif  // KV_CLI_HPP
