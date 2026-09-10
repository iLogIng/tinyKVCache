#include "kv/net.hpp"

#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

namespace kv {
namespace {

// 小端写 u32
void put_le32(std::string& s, std::uint32_t v)
{
    s.push_back(static_cast<char>(v & 0xff));
    s.push_back(static_cast<char>((v >> 8) & 0xff));
    s.push_back(static_cast<char>((v >> 16) & 0xff));
    s.push_back(static_cast<char>((v >> 24) & 0xff));
}

// 小端读 u32
// 越界 return false
bool get_le32(std::string_view s, std::size_t off, std::uint32_t& out)
{
    if (off + 4 > s.size()) {
        return false;
    }
    out = static_cast<std::uint8_t>(s[off])
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[off + 1])) << 8)
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[off + 2])) << 16)
        | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[off + 3])) << 24);
    return true;
}

// 读 n 字节
bool read_exact(int fd, char* buf, std::size_t n)
{
    std::size_t got = 0;
    while (got < n) {
        const ssize_t r = ::recv(fd, buf + got, n - got, 0);
        if (r > 0) {
            got += static_cast<std::size_t>(r);
        }
        else if (r == 0) {
            return false;  // EOF
        }
        else if (errno == EINTR) {
            continue;
        }
        else {
            return false;
        }
    }
    return true;
}

}  // namespace

// 发送数据
bool send_all(int fd, const std::string& data)
{
    std::size_t sent = 0;
    // 连续发送 data.size() 字节的数据
    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
        }
        else if (n < 0 && errno == EINTR) {
            continue;
        }
        else {
            return false;
        }
    }
    return true;
}

std::string encode_request(std::string_view cmd,
                           const std::vector<std::string>& args)
{
    if (cmd.empty() || cmd.size() > 255 || args.size() > 255) {
        return {};
    }
    std::string body;
    body.reserve(2 + cmd.size() + args.size() * 8);
    body.push_back(static_cast<char>(args.size()));
    body.push_back(static_cast<char>(cmd.size()));
    body.append(cmd.data(), cmd.size());
    for (const std::string& arg : args) {
        if (arg.size() > 0xffffffffu) {
            return {};
        }
        put_le32(body, static_cast<std::uint32_t>(arg.size()));
        body.append(arg);
    }
    if (body.size() > kMaxFrame) {
        return {};
    }
    return body;
}

bool decode_request(std::string_view body, std::string& cmd,
                    std::vector<std::string>& args)
{
    if (body.size() < 2) {
        return false;
    }
    const std::size_t cmdargs = static_cast<std::uint8_t>(body[0]);
    const std::size_t cmd_len = static_cast<std::uint8_t>(body[1]);
    if (cmd_len == 0) {
        return false;
    }
    std::size_t off = 2;
    if (off + cmd_len > body.size()) {
        return false;
    }
    cmd.assign(body.data() + off, cmd_len);
    off += cmd_len;

    args.clear();
    args.reserve(cmdargs);
    for (std::size_t i = 0; i < cmdargs; ++i) {
        std::uint32_t len = 0;
        if (!get_le32(body, off, len)) {
            return false;
        }
        off += 4;
        if (off + len > body.size()) {
            return false;
        }
        args.emplace_back(body.data() + off, len);
        off += len;
    }
    return true;
}

std::string encode_frame(std::string_view body)
{
    if (body.size() > kMaxFrame) {
        return {};
    }
    std::string frame;
    frame.reserve(5 + body.size());
    frame.push_back(static_cast<char>(kProtocolVersion));
    put_le32(frame, static_cast<std::uint32_t>(body.size()));
    frame.append(body.data(), body.size());
    return frame;
}

bool write_frame(int fd, std::string_view body)
{
    const std::string frame = encode_frame(body);
    if (frame.empty()) {
        return false;
    }
    return send_all(fd, frame);
}

bool read_frame(int fd, std::string& body)
{
    std::uint8_t ver = 0;
    // 1B version
    if (!read_exact(fd, reinterpret_cast<char*>(&ver), 1)) {
        return false;
    }
    if (ver != kProtocolVersion) {
        return false;
    }
    // 4B body len
    char len_buf[4];
    if (!read_exact(fd, len_buf, 4)) {
        return false;
    }
    std::uint32_t len = 0;
    get_le32(std::string_view(len_buf, 4), 0, len);
    if (len > kMaxFrame) {
        return false;
    }
    body.resize(len);
    if (len > 0 && !read_exact(fd, body.data(), len)) {
        return false;
    }
    return true;
}

}  // namespace kv
