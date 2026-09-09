#ifndef KV_REGCMD_HPP
#define KV_REGCMD_HPP

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "kv/cli.hpp"
#include "kv/engine.hpp"

namespace kv {

// 查表校验并执行一条命令。
// 成功: 结果写入 out, 返回 nullopt; 失败: 返回错误文本(供本地打印), out 不变。
std::optional<std::string> exec(Engine& engine,
                                const std::vector<std::string>& tokens,
                                std::ostream& out);

}  // namespace kv

#endif  // KV_REGCMD_HPP
