#include "kv/engine.hpp"
#include "kv/journal.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace kv {

namespace {
// 日志追加失败的兜底提示
void warn_journal_fail()
{
    std::cerr << "engine: journal append failed\n";
}
}  // namespace

Engine::Engine(std::size_t capacity) : capacity_(capacity)
{
    if (capacity_ == 0) {
        throw std::invalid_argument("capacity must be > 0");
    }
    cache.resize(capacity_);
    frstk.reserve(capacity_);
    for (int i = static_cast<int>(capacity_) - 1; i >= 0; --i) {
        frstk.push_back(i);
    }
}

void Engine::put(const std::string& key, const std::string& value)
{
    if (journal_ != nullptr && !journal_->append(Op::Put, key, value)) {
        warn_journal_fail();
    }
    auto it = catalog.find(key);
    // 已存在: 覆盖并刷新为最近使用
    if (it != catalog.end()) {
        int idx = it->second;
        cache[idx].value = value;
        if (idx != head_) {
            unlink(idx);
            push_front(idx);
        }
        return;
    }

    int idx;
    if (!frstk.empty()) {
        // 有空槽: 取一个
        idx = frstk.back();
        frstk.pop_back();
    }
    else {
        // 满: 淘汰最久未用, 复用其槽
        idx = tail_;
        std::string victim = std::move(cache[idx].key);
        catalog.erase(victim);
        unlink(idx);
    }

    cache[idx].key = key;
    cache[idx].value = value;
    catalog[key] = idx;
    push_front(idx);
}

std::optional<std::string> Engine::get(const std::string& key)
{
    auto it = catalog.find(key);
    if (it == catalog.end()) {
        return std::nullopt;
    }
    int idx = it->second;
    if (idx != head_) {
        unlink(idx);
        push_front(idx);
    }
    return cache[idx].value;
}

bool Engine::erase(const std::string& key)
{
    auto it = catalog.find(key);
    if (it == catalog.end()) {
        return false;
    }
    if (journal_ != nullptr && !journal_->append(Op::Del, key, "")) {
        warn_journal_fail();
    }
    int idx = it->second;
    unlink(idx);
    catalog.erase(it);
    frstk.push_back(idx);
    cache[idx].key.clear();
    cache[idx].value.clear();
    return true;
}

void Engine::clear() noexcept
{
    if (journal_ != nullptr && !journal_->append(Op::Clear, "", "")) {
        warn_journal_fail();
    }
    catalog.clear();
    for (Slot& s : cache) {
        s.key.clear();
        s.value.clear();
        s.prev = -1;
        s.next = -1;
    }
    frstk.clear();
    frstk.reserve(capacity_);
    for (int i = 0; i < static_cast<int>(capacity_); ++i) {
        frstk.push_back(i);
    }
    head_ = -1;
    tail_ = -1;
}

std::size_t Engine::size() const noexcept
{
    return catalog.size();
}

std::size_t Engine::capacity() const noexcept
{
    return capacity_;
}

std::vector<std::pair<std::string, std::string>> Engine::items() const
{
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(catalog.size());
    for (int i = head_; i != -1; i = cache[i].next) {
        out.emplace_back(cache[i].key, cache[i].value);
    }
    return out;
}

void Engine::unlink(int idx) noexcept
{
    int prev = cache[idx].prev;
    int next = cache[idx].next;
    if (prev != -1) {
        cache[prev].next = next;
    }
    else {
        head_ = next;
    }
    if (next != -1) {
        cache[next].prev = prev;
    }
    else {
        tail_ = prev;
    }
    cache[idx].prev = -1;
    cache[idx].next = -1;
}

void Engine::push_front(int idx) noexcept
{
    cache[idx].prev = -1;
    cache[idx].next = head_;
    if (head_ != -1) {
        cache[head_].prev = idx;
    }
    else {
        tail_ = idx;
    }
    head_ = idx;
}

void Engine::attach(Journal& journal)
{
    journal_ = &journal;
}

void Engine::detach() noexcept
{
    journal_ = nullptr;
}

bool Engine::replay(Journal& journal)
{
    Journal* saved = journal_;
    journal_ = nullptr;  // 回放期间不把重建过程再写回日志
    const bool ok = journal.replay([this](const Record& r) {
        switch (r.op) {
            case Op::Put:
                put(r.key, r.value);
                break;
            case Op::Del:
                erase(r.key);
                break;
            case Op::Clear:
                clear();
                break;
        }
    });
    journal_ = saved;
    return ok;
}

}  // namespace kv
