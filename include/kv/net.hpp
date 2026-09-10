#ifndef KV_NET_HPP
#define KV_NET_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kv {

constexpr unsigned short kDefaultPort = 7379;
constexpr std::size_t kMaxLine = 64 * 1024;  // 单请求/响应行上限
constexpr std::uint8_t kProtocolVersion = 1;
constexpr std::size_t kMaxFrame = 1024 * 1024;  // 单帧 body 上限

// 从阻塞 fd 读一行(不含 '\n')。EOF/出错返回 false，超长返回 false
bool read_line(int fd, std::string& out);

// 发完全部字节。返回 false 表示出错
bool send_all(int fd, const std::string& data);

// 读一个"空行终止"的响应帧(不含终止空行)。完整读到一个空行才返回 true
bool recv_frame(int fd, std::string& out);

// 编码请求 body: [1B cmdargs][1B cmd_len][cmd][4B len+bytes]*cmdargs
// 参数非法时返回空串
std::string encode_request(std::string_view cmd,
                           const std::vector<std::string>& args);

// 解码请求 body; 读完 cmdargs 个参数后忽略尾部多余字节
bool decode_request(std::string_view body, std::string& cmd,
                    std::vector<std::string>& args);

// 帧: [1B ver][4B body_len][body]
bool write_frame(int fd, std::string_view body);
bool read_frame(int fd, std::string& body);

}  // namespace kv

#endif  // KV_NET_HPP
