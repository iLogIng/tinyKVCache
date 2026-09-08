#include "kv/engine.hpp"

namespace kv {

void Engine::put(const std::string& key, const std::string& value)
{
    cache[key] = value;
}

std::optional<std::string> Engine::get(const std::string& key) const
{
    auto it = cache.find(key);
    if (it == cache.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool Engine::erase(const std::string& key)
{
    return cache.erase(key) > 0;
}

void Engine::clear() noexcept
{
    cache.clear();
}

std::size_t Engine::size() const noexcept
{
    return cache.size();
}

std::vector<std::pair<std::string, std::string>> Engine::items() const
{
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(cache.size());
    for (const auto& [key, value] : cache) {
        out.emplace_back(key, value);
    }
    return out;
}

}  // namespace kv
