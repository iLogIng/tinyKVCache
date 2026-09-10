#ifndef KV_JOURNAL_HPP
#define KV_JOURNAL_HPP

#include <chrono>
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
    Always, Group, Os
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

    // 立即刷盘(仅当有未刷数据)
    bool sync();

    // 是否需要刷盘
    // 有未刷数据且距上次刷盘已超过 interval
    bool dirty_or_interval_sync(
        std::chrono::milliseconds interval,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;

    // 距下次刷盘剩余时间
    // 无未刷数据返回 -1ms, 已到点返回 0ms
    std::chrono::milliseconds time_until_sync(
        std::chrono::milliseconds interval,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;

    std::uint64_t write_count() const { return write_count_; }
    std::uint64_t write_fail_count() const { return write_fail_count_; }

    const std::string& path() const { return path_; }

private:
    int fd_ = -1;       // aof文件描述符
    std::string path_;  // aof文件路径
    Fsync fsync_policy_ = Fsync::Always;        // fsync策略
    bool dirty_ = false;                        // 是否同步?
    std::uint64_t write_count_ = 0;             // 写计数
    std::uint64_t write_fail_count_ = 0;        // 写失败记录
    std::chrono::steady_clock::time_point last_fsync_;  // 最近同步时间
};

}  // namespace kv

#endif  // KV_JOURNAL_HPP
