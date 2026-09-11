#include "kv/resp2.hpp"

namespace kv {
namespace {

// 读取一行(不含 \r\n), pos 前移到行后; 无完整行返回 false
bool read_line(std::string_view buf, std::size_t& pos, std::string_view& line)
{
    const std::size_t p = buf.find("\r\n", pos);
    if (p == std::string_view::npos) {
        return false;
    }
    line = buf.substr(pos, p - pos);
    pos = p + 2;
    return true;
}

// 解析十进制整数
bool parse_int(std::string_view s, long long& out)
{
    if (s.empty()) {
        return false;
    }
    std::size_t i = 0;
    bool neg = false;
    if (s[0] == '-') {
        neg = true;
        i = 1;
        if (i == s.size()) {
            return false;
        }
    }
    long long v = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        v = v * 10 + (s[i] - '0');
        if (v > 1000000000000LL) {
            return false;
        }
    }
    out = neg ? -v : v;
    return true;
}

}  // namespace

Resp2Status resp2_parse(std::string_view buf, std::size_t& off,
                        std::vector<std::string>& args)
{
    if (off >= buf.size()) {
        return Resp2Status::Incomplete;
    }
    if (buf[off] != '*') {
        return Resp2Status::Invalid;
    }

    std::size_t pos = off;
    std::string_view line;
    if (!read_line(buf, pos, line)) {
        return Resp2Status::Incomplete;
    }
    long long n = 0;
    if (line.size() < 2 || !parse_int(line.substr(1), n) || n <= 0) {
        return Resp2Status::Invalid;
    }

    std::vector<std::string> out;
    out.reserve(static_cast<std::size_t>(n));
    for (long long i = 0; i < n; ++i) {
        if (pos >= buf.size()) {
            return Resp2Status::Incomplete;
        }
        if (buf[pos] != '$') {
            return Resp2Status::Invalid;
        }
        std::size_t q = pos;
        std::string_view len_line;
        if (!read_line(buf, q, len_line)) {
            return Resp2Status::Incomplete;
        }
        long long len = 0;
        if (len_line.size() < 2 || !parse_int(len_line.substr(1), len) || len < 0) {
            return Resp2Status::Invalid;
        }
        if (buf.size() - q < static_cast<std::size_t>(len) + 2) {
            return Resp2Status::Incomplete;
        }
        if (buf[q + len] != '\r' || buf[q + len + 1] != '\n') {
            return Resp2Status::Invalid;
        }
        out.emplace_back(buf.substr(q, static_cast<std::size_t>(len)));
        pos = q + static_cast<std::size_t>(len) + 2;
    }

    args = std::move(out);
    off = pos;
    return Resp2Status::Ok;
}

std::string resp2_simple(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 3);
    out.push_back('+');
    out.append(s);
    out.append("\r\n");
    return out;
}

std::string resp2_error(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 3);
    out.push_back('-');
    out.append(s);
    out.append("\r\n");
    return out;
}

std::string resp2_integer(long long v)
{
    return ':' + std::to_string(v) + "\r\n";
}

std::string resp2_bulk(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 16);
    out.push_back('$');
    out.append(std::to_string(s.size()));
    out.append("\r\n");
    out.append(s);
    out.append("\r\n");
    return out;
}

std::string resp2_null_bulk()
{
    return "$-1\r\n";
}

}  // namespace kv
