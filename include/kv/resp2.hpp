#ifndef KV_RESP2_HPP
#define KV_RESP2_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace kv {

enum class Resp2Status
{
    Ok, Incomplete, Invalid
};

// 解析一条命令(数组 + 批量字符串)到 args; 成功时前移 off
Resp2Status resp2_parse(std::string_view buf, std::size_t& off,
                        std::vector<std::string>& args);

// 序列化应答
std::string resp2_simple(std::string_view s);   // +s\r\n
std::string resp2_error(std::string_view s);    // -s\r\n
std::string resp2_integer(long long v);         // :v\r\n
std::string resp2_bulk(std::string_view s);     // $len\r\ns\r\n
std::string resp2_null_bulk();                  // $-1\r\n

}  // namespace kv

#endif  // KV_RESP2_HPP
