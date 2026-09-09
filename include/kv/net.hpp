#ifndef KV_NET_HPP
#define KV_NET_HPP

#include <cstddef>
#include <string>

namespace kv {

constexpr unsigned short kDefaultPort = 7379;
constexpr std::size_t kMaxLine = 64 * 1024;  // 单请求/响应行上限

// 从阻塞 fd 读一行(不含 '\n')。EOF/出错返回 false，超长返回 false
bool read_line(int fd, std::string& out);

// 发完全部字节。返回 false 表示出错
bool send_all(int fd, const std::string& data);

// 读一个"空行终止"的响应帧(不含终止空行)。完整读到一个空行才返回 true
bool recv_frame(int fd, std::string& out);

}  // namespace kv

#endif  // KV_NET_HPP
