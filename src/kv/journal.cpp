#include "kv/journal.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

namespace kv {
namespace {

// 小端写 u32 到缓冲
void put_le32(std::string& s, uint32_t v)
{
    s.push_back(static_cast<char>(v & 0xff));
    s.push_back(static_cast<char>((v >> 8) & 0xff));
    s.push_back(static_cast<char>((v >> 16) & 0xff));
    s.push_back(static_cast<char>((v >> 24) & 0xff));
}

// 从缓冲读小端 u32, 越界返回 false
bool get_le32(const std::string& s, std::size_t off, uint32_t& out)
{
    if (off + 4 > s.size()) {
        return false;
    }
    out = static_cast<uint8_t>(s[off])
        | (static_cast<uint32_t>(static_cast<uint8_t>(s[off + 1])) << 8)
        | (static_cast<uint32_t>(static_cast<uint8_t>(s[off + 2])) << 16)
        | (static_cast<uint32_t>(static_cast<uint8_t>(s[off + 3])) << 24);
    return true;
}

}  // namespace

bool Journal::open(const std::string& path, Fsync fsync_policy)
{
    // 创建时，仅由创建用户可读可写
    // O_APPEND: 每次写入原子定位到文件尾
    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_APPEND, 0600);
    if (fd_ < 0) {
        std::cerr << "journal: open '" << path << "': " << std::strerror(errno) << '\n';
        return false;
    }
    path_ = path;
    fsync_policy_ = fsync_policy;
    return true;
}

void Journal::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool Journal::append(Op op, std::string_view key, std::string_view value)
{
    if (fd_ < 0) {
        return false;
    }
    std::string rec;
    // 1B cmdop 4B key-str 4B value-str
    rec.reserve(1 + 4 + key.size() + 4 + value.size());
    rec.push_back(static_cast<char>(op));
    put_le32(rec, static_cast<uint32_t>(key.size()));
    rec.append(key.data(), key.size());
    put_le32(rec, static_cast<uint32_t>(value.size()));
    rec.append(value.data(), value.size());

    std::size_t done = 0; // 检测写入是否完成
    // 从缓冲写入文件
    while (done < rec.size()) {
        const ssize_t n = ::write(fd_, rec.data() + done, rec.size() - done);
        if (n > 0) {
            done += static_cast<std::size_t>(n);
        }
        else if (n < 0 && errno == EINTR) {
            continue;
        }
        else {
            return false;
        }
    }
    if (fsync_policy_ == Fsync::Always && ::fsync(fd_) < 0) {
        return false;
    }
    return true;
}

// 使用on_record响应重建记录
bool Journal::replay(const std::function<void(const Record&)>& on_record)
{
    if (fd_ < 0) {
        return false;
    }
    if (::lseek(fd_, 0, SEEK_SET) < 0) {
        return false;
    }
    // 数据
    std::string data;
    // 缓存
    char buf[8192];
    for (;;) {
        // 读取缓存
        const ssize_t n = ::read(fd_, buf, sizeof(buf));
        if (n > 0) {
            // 将缓存写入数据
            data.append(buf, static_cast<std::size_t>(n));
        }
        else if (n == 0) {
            break;
        }
        else if (errno == EINTR) {
            continue;
        }
        else {
            return false;
        }
    }

    std::size_t off = 0;
    std::size_t valid = 0;
    while (off < data.size()) {
        if (data.size() - off < 5) {
            break;  // 尾部不足 kind+klen
        }
        const Op op = static_cast<Op>(static_cast<uint8_t>(data[off]));
        if (op != Op::Put && op != Op::Del && op != Op::Clear) {
            break;  // 未知类型视为坏记录
        }
        uint32_t klen = 0;
        if (!get_le32(data, off + 1, klen)) {
            break;
        }
        // 布局: [op kind][klen][key][vlen][value]
        const std::size_t vlen_off = off + 1 + 4 + klen;
        if (vlen_off + 4 > data.size()) {
            break;  // key 数据不完整
        }
        uint32_t vlen = 0;
        if (!get_le32(data, vlen_off, vlen)) {
            break;
        }
        // 记录后的末尾
        const std::size_t rec_end = vlen_off + 4 + vlen;
        if (rec_end > data.size()) {
            break;  // 尾部记录不完整
        }
        // 记录结构
        Record r;
        r.op = op;
        r.key.assign(data, off + 5, klen);
        r.value.assign(data, vlen_off + 4, vlen);
        on_record(r);
        off = rec_end;
        valid = rec_end;
    }
    if (off < data.size()) {
        // 丢弃坏尾并截断
        if (::ftruncate(fd_, static_cast<off_t>(valid)) < 0) {
            return false;
        }
    }
    return true;
}

}  // namespace kv
