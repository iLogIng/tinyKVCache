#ifndef KV_ENGINE_HPP
#define KV_ENGINE_HPP

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kv {

class Journal;

// 槽位:
// key/value 侵入式双链
// 下标代替指针, -1 表示空
struct Slot
{
    std::string key;
    std::string value;
    int prev = -1;
    int next = -1;
};

// 内存键值引擎: 有界缓存, 存满时淘汰最久未用(LRU)
class Engine
{
public:
    explicit Engine(std::size_t capacity);   // capacity > 0
    Engine(const Engine&) = delete;
    Engine& operator =(const Engine&) = delete;

    // 新增或覆盖; 满时淘汰最久未用
    void put(const std::string& key, const std::string& value);

    // 读取, key 不存在时返回 nullopt; 命中刷新为最近使用
    std::optional<std::string> get(const std::string& key);

    // 删除, 返回 key 是否原本存在
    bool erase(const std::string& key);

    void clear() noexcept;

    std::size_t size() const noexcept;

    std::size_t capacity() const noexcept;

    // 全量快照(按 LRU 排序), 供遍历输出
    std::vector<std::pair<std::string, std::string>> items() const;

    // 挂接持久化日志: 写操作先记日志再改内存
    void attach(Journal& journal);

    void detach() noexcept;

    // 从日志回放写序列重建内存(回放期间不自我记日志)
    bool replay(Journal& journal);

private:
    void unlink(int idx) noexcept;      // 从链上摘下 idx
    void push_front(int idx) noexcept;  // 挂 idx 为最近使用

    std::size_t capacity_;
    std::unordered_map<std::string, int> catalog;  // 键-索引 映射
    std::vector<Slot> cache;    // 槽位池: 数据 与 LRU 链
    std::vector<int> frstk;     // 空闲槽栈
    int head_ = -1;  // 链端点 MRU
    int tail_ = -1;  // 链端点 LRU
    // 重放日志，在engine的既有命令方法中直接记录
    Journal* journal_ = nullptr;
};

}  // namespace kv

#endif  // KV_ENGINE_HPP
