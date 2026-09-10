#ifndef KV_NET_HPP
#define KV_NET_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kv {

constexpr unsigned short kDefaultPort = 7379;   // 默认占用端口
constexpr std::uint8_t kProtocolVersion = 1;    // 协议版本
constexpr std::size_t kMaxFrame = 1024 * 1024;  // 单帧 body 字节量上限

// 发送全部字节。
bool send_all(int fd, const std::string& data);

// 编码请求体
// [1B cmdargs][1B cmd_len][cmd] [4B len+bytes]*cmdargs
// 参数非法时返回空串
std::string encode_request(std::string_view cmd,
                           const std::vector<std::string>& args);

// 解码请求体
// 读取 cmdargs 个参数后忽略尾部多余字节
bool decode_request(std::string_view body, std::string& cmd,
                    std::vector<std::string>& args);

// 编码帧
// [1B version][4B body_len][body]
std::string encode_frame(std::string_view body);

// 帧读写
// [1B ver][4B body_len][body]
bool write_frame(int fd, std::string_view body);
bool read_frame(int fd, std::string& body);

}  // namespace kv

#endif  // KV_NET_HPP
