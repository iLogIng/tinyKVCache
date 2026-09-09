#include "kv/cli.hpp"

#include <utility>

namespace kv {

// "cmdop" "key" "value" 解析器
std::vector<std::string> tokenize(const std::string& line)
{
    std::vector<std::string> out;
    std::string cur;
    bool in_quote = false;
    bool started = false;

    // 脱去 命令、参数 两边的引号
    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        // 引号内部
        if (in_quote) {
            // 引号结束
            if (ch == '"') {
                in_quote = false;
            }
            else if (ch == '\\' && i + 1 < line.size()) { // 转译
                cur.push_back(line[++i]);
            }
            else {
                cur.push_back(ch);
            }
        }
        else if (ch == '"') { // 引号开始
            in_quote = true;
            started = true;
        }
        else if (ch == ' ' || ch == '\t') { // 开始新的 key "value"
            // 将解析的 key/value 追加为命令参数
            if (started) {
                out.push_back(std::move(cur));
                cur.clear();
                started = false;
            }
        }
        else if (ch == '\\' && i + 1 < line.size()) { // 转译
            cur.push_back(line[++i]);
            started = true;
        }
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
ParseError parse(const std::vector<std::string>& tokens, Command& out)
{
    if (tokens.empty()) {
        return ParseError::Ok;
    }

    // 查找命令
    const CommandSpec* spec = nullptr;
    for (size_t i = 0; kSpecs[i].name.size() > 0; ++i) {
        if (kSpecs[i].name == tokens[0]) {
            spec = &kSpecs[i];
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

    out.name = spec->name;
    out.args.assign(tokens.begin() + 1, tokens.end());
    return ParseError::Ok;
}

// 命令说明
std::string_view usage_of(std::string_view name)
{
    // 遍历查找
    for (size_t i = 0; kSpecs[i].name.size() > 0; ++i) {
        if (kSpecs[i].name == name) {
            return kSpecs[i].usage;
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
