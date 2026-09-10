#ifndef KV_JOURNAL_HPP
#define KV_JOURNAL_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace kv {

enum class Op : uint8_t
{
    Put, Del, Clear
};

enum class Fsync
{
    Always, Os
};

struct Record {
    Op op;
    std::string key;
    std::string value;
};

// 缓存文件管理模块
// 追加操作记录
class Journal {
public:
    Journal() = default;
    Journal(const Journal&) = delete;
    Journal& operator=(const Journal&) = delete;
    ~Journal() { close(); }

    // (创建)打开日志文件; 按 fsync 策略执行
    bool open(const std::string& path, Fsync fsync_policy = Fsync::Always);

    void close();

    // 追加一条操作记录
    bool append(Op op, std::string_view key, std::string_view value);

    // 顺序读取全部记录并逐条回调; 尾部不完整记录截断恢复
    bool replay(const std::function<void(const Record&)>& on_record);

    const std::string& path() const { return path_; }

private:
    int fd_ = -1;
    std::string path_;
    Fsync fsync_policy_ = Fsync::Always;
};

}  // namespace kv

#endif  // KV_JOURNAL_HPP
