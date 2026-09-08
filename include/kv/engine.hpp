#ifndef KV_ENGINE_HPP
#define KV_ENGINE_HPP

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kv {

// 内存键值引擎，key/value 均为 string
class Engine
{
public:
    // 新增或覆盖
    void put(const std::string& key, const std::string& value);

    // 读取，key 不存在时返回 nullopt
    std::optional<std::string> get(const std::string& key) const;

    // 删除，返回 key 是否原本存在
    bool erase(const std::string& key);

    void clear() noexcept;

    std::size_t size() const noexcept;

    // 全量快照，供遍历输出
    std::vector<std::pair<std::string, std::string>> items() const;

private:
    std::unordered_map<std::string, std::string> cache;
};

}  // namespace kv

#endif  // KV_ENGINE_HPP
