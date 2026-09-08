#ifndef KV_REGCMD_HPP
#define KV_REGCMD_HPP

#include <string>
#include <vector>

#include "kv/cli.hpp"
#include "kv/engine.hpp"

namespace kv {

/* 查表校验并执行一条命令。
   返回 0 成功，2 为解析/参数错误（已打印到 stderr），绝不退出进程。 */
int exec(Engine& engine, const std::vector<std::string>& tokens);

}  // namespace kv

#endif  // KV_REGCMD_HPP
