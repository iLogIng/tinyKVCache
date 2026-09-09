#include "kv/cli.hpp"

#include <utility>

namespace kv {

// "cmdop" "key" "value" 解析器
std::vector<std::string> tokenize(const std::string& line)
{
    std::vector<std::string> out;
    std::string cur;
    char quote_char = 0;    // 记录目前的引号类型 " / '
    bool started = false;

    // 脱去 命令、参数 两边的 单、双引号
    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        // 字符转译
        if (ch == '\\' && i + 1 < line.size()) {
            cur.push_back(line[++i]);
            started = true;
        }
        // 引号内
        else if (ch == '"' || ch == '\'') {
            // 进入引号模式
            if (quote_char == 0) {
                quote_char = ch;
                started = true;   // 保空 token: `""` 也要作为一个参数
            }
            // 退出引号模式
            else if (quote_char == ch) {
                quote_char = 0;
            }
            else {
                cur.push_back(ch);
                started = true;
            }
        }
        // 在引号外 空白字符作为分隔符
        else if (quote_char == 0 && (ch == ' ' || ch == '\t')) {
            if (started) {
                out.push_back(std::move(cur));
                cur.clear();
                started = false;
            }
        }
        // 记录普通字符
        else {
            cur.push_back(ch);
            started = true;
        }
    }

    if (started) {
        out.push_back(std::move(cur));
    }
    return out;
}

// 解析
// token: cmdop argv...
ParseError parse(const std::vector<std::string>& tokens, Command& out)
{
    if (tokens.empty()) {
        return ParseError::Ok;
    }

    // 查找命令
    const CommandSpec* spec = nullptr;
    for (const auto& k : commands()) {
        if (k.name == tokens[0]) {
            spec = &k;
            break;
        }
    }
    if (spec == nullptr) {
        return ParseError::UnknownCommand;
    }

    // 验证命令参数数量
    const int argc = static_cast<int>(tokens.size()) - 1;
    if (argc < spec->min_args || argc > spec->max_args) {
        return ParseError::WrongArgCount;
    }

    // 传出
    out.name = spec->name;
    out.args.assign(tokens.begin() + 1, tokens.end());
    return ParseError::Ok;
}

// 命令说明
std::string_view usage_of(std::string_view name)
{
    // 遍历查找
    for (const auto& k : commands()) {
        if (k.name == name) {
            return k.usage;
        }
    }
    return {};
}

// 解析器错误信息
const char* to_string(ParseError err)
{
    switch (err) {
        case ParseError::Ok:
            return "ok";
        case ParseError::UnknownCommand:
            return "unknown command";
        case ParseError::WrongArgCount:
            return "wrong argument count";
    }
    return "unknown error";
}

}  // namespace kv
