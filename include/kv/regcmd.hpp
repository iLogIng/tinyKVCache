#ifndef KV_REGCMD_HPP
#define KV_REGCMD_HPP

#include <iosfwd>
#include <string>
#include <vector>

#include "kv/cli.hpp"
#include "kv/engine.hpp"

namespace kv {

// 查表校验并执行一条命令。
// 成功: 结果写入 out, 返回 0; 失败: 错误打印到 std::cerr, 返回 2。
int exec(Engine& engine, const std::vector<std::string>& tokens,
         std::ostream& out);

}  // namespace kv

#endif  // KV_REGCMD_HPP
